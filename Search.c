#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <omp.h>

// Tiles
#define FLOOR       0x00
#define WALL        0x01
#define COSMIC_CHIP 0x02
#define EXIT        0x15
#define GRAVEL      0x2D
#define BLOB_N      0x5C
#define CHIP_S      0x6E

enum {
    NORTH = 0,
    EAST = 1,
    SOUTH = 2,
    WEST = 3
};

enum {
    ODD = 0,
    EVEN =1
};

/*
 * How each move is represented via the index position system
 */
#define MOVE_DOWN    32
#define MOVE_UP     -32
#define MOVE_RIGHT    1
#define MOVE_LEFT    -1

typedef signed char DIR;
static const DIR diridx[4] = { MOVE_UP, MOVE_RIGHT, MOVE_DOWN, MOVE_LEFT };

struct prng {
    uint32_t value;
};

static void advance(struct prng *rng);
static int randn(struct prng *rng, int max);

typedef struct BLOB {
    int index;
    DIR dir;
} BLOB;

static bool verifyRoute(void);
static int canEnter(unsigned char tile);
static void moveBlob(struct prng*, BLOB* b, unsigned char upper[]);
static void moveChip(char dir, int *chipIndex, unsigned char upper[]);
static void searchSeed(unsigned long seed, int step, uint64_t* nummoves);

static char* route;
static int routeLength = 0;

#define NUM_BLOBS 80
static BLOB monsterListInitial[NUM_BLOBS]; //80 blobs in the level, the list simply keeps track of the current position/index of each blob as it appears in the level (order is of course that of the monster list)
static unsigned char mapInitial[1024];
static int chipIndexInitial;

// See https://flak.tedunangst.com/post/embedding-binary-objects-in-c
#ifdef WIN32
#define _binary_blobnet_bin_start binary_blobnet_bin_start
#define _binary_blobnet_bin_size binary_blobnet_bin_size
#endif
extern const void _binary_blobnet_bin_start;
extern const void _binary_blobnet_bin_size;
const char* blobnet_start = (char*)&_binary_blobnet_bin_start;
const size_t blobnet_size = (size_t)&_binary_blobnet_bin_size;

int main(int argc, const char* argv[]) {
    if (argc == 1) {
        printf("Please enter the filename for the route to test\n");
        return 0;
    }

    if (blobnet_size != 2*sizeof(mapInitial)) {
        printf("internal error: size mismatch in map (want %lu, found %lu)\n", (unsigned long)2*sizeof(mapInitial), (unsigned long)blobnet_size);
        return 1;
    }
    memmove(mapInitial, blobnet_start, sizeof(mapInitial));

    FILE *file;
    file = fopen(argv[1], "rb"); //The route filename should be provided via command line
    char character;
    while ((character = fgetc(file)) != EOF) { //Sharpeye's code for getting a file size that doesn't rely on SEEK_END
        routeLength++; //EOF is not a part of the original file and therefore incrementing the variable even after hitting means that the variable is equal to the file size
    }
    rewind(file);
    //The search loop reads two moves at a time, so add some padding at the end so we don't read past the array bounds.
    route = calloc(routeLength+10, sizeof(char)); //Create an array who's size is based on the route file size, calloc o inits the meory area
    fread(route, routeLength, 1, file);
    fclose(file);

    int listIndex = 0; //Put all the blobs into a list
    for (int c = 0; c < (int)sizeof(mapInitial); c++) {
        char tile = mapInitial[c];
        switch (tile) {
            case(BLOB_N): ;
                if (listIndex < NUM_BLOBS) {
                    BLOB b = {c, NORTH}; //All the blobs are facing up
                    monsterListInitial[listIndex] = b;
                    listIndex++;
                } else {
                    mapInitial[c] = FLOOR;
                }
                break;
            case(CHIP_S): //Chip-S, the only Chip tile that appears in blobnet
                chipIndexInitial = c;
                mapInitial[c] = GRAVEL; //Set the tile to gravel for map consistency once Chip is removed
                break;
        }
    }

    for (int r = 0; r < routeLength; r++) {
        char direction = route[r];
        switch (direction) {
            case ('d'):
                route[r] = MOVE_DOWN;
                break;
            case ('u'):
                route[r] = MOVE_UP;
                break;
            case ('r'):
                route[r] = MOVE_RIGHT;
                break;
            case ('l'):
                route[r] = MOVE_LEFT;
                break;
            case ('-'):
                route[r] = 0;
                break;
            default:
                printf("ILLEGAL DIRECTION CHARACTER AT ROUTE POSITION %d, CHARACTER: %c\n", r, direction);
                route[r] = 0; //don't want things getting messed up
                break;
        }
    }

    if (!verifyRoute()) {
        printf("invalid route\n");
        return 1;
    }

    long numThreads = omp_get_num_procs();
    omp_set_num_threads(numThreads);
    printf("using %ld threads\n", numThreads);

    unsigned long firstSeed = 0;
    unsigned long lastSeed = SHRT_MAX;
    //firstSeed = 574199820 - 50000;
    //lastSeed = 574199820 + 50000;

    double time_a = omp_get_wtime();

    uint64_t nummoves = 0;
    #pragma omp parallel for default(none) shared(firstSeed,lastSeed),reduction(+:nummoves)
    for (unsigned long seed = firstSeed; seed <= lastSeed; seed++) {
        //int threadNum = omp_get_thread_num();
        searchSeed(seed, EVEN, &nummoves);
        searchSeed(seed, ODD, &nummoves);
    }

    // printf("Thread #%ld: start=%#lx\tend=%#lx\n", threadNum, poolInfo->poolStart, poolInfo->poolEnd);

    unsigned long numSeeds = lastSeed - firstSeed + 1;
    double time_b = omp_get_wtime();
    double duration = time_b - time_a;

    printf("searched %lu seeds in %f ms\n", numSeeds, duration * 1e3);
    printf("played %lu moves\n", (unsigned long)nummoves);
    printf("average %.1f us/seed\n", duration * 1e6 / numSeeds);
    printf("average %.1f moves/seed\n", (double)nummoves / numSeeds);
}

static bool verifyRoute(void) {
    int chipIndex = chipIndexInitial;
    unsigned char map[1024];
    BLOB monsterList[NUM_BLOBS];
    memcpy(map, mapInitial, 1024);
    memcpy(monsterList, monsterListInitial, sizeof(struct BLOB)*NUM_BLOBS);

    int chipsNeeded = 88;
    int ok = true;
    for (int i = 0; i < routeLength; i++) {
        int dir = route[i];
        chipIndex = chipIndex + dir;
        if (map[chipIndex] == WALL) {
            printf("hit a wall at move %d\n", i);
            ok = false;
            break;
        }
        if (map[chipIndex] == COSMIC_CHIP) {
            map[chipIndex] = FLOOR;
            chipsNeeded -= 1;
        }
    }
    if (chipsNeeded > 0) {
        printf("route does not collect all chips\n");
        ok = false;
    }
    if (map[chipIndex] != EXIT) {
        printf("route does not reach the exit\n");
        ok = false;
    }
    return ok;
}

static void searchSeed(unsigned long startingSeed, int step, uint64_t *nummoves) { //Step: 1 = EVEN, 0 = ODD
    struct prng rng = {startingSeed};
    int chipIndex = chipIndexInitial;
    unsigned char map[1024];
    BLOB monsterList[NUM_BLOBS];
    memcpy(map, mapInitial, 1024);
    memcpy(monsterList, monsterListInitial, sizeof(struct BLOB)*NUM_BLOBS); //Set up copies of the arrays to be used so we don't have to read from file each time

    if (step == EVEN) {
        moveChip(route[0], &chipIndex, map);
    }
    int i=step;
    while (i < routeLength) {
        moveChip(route[i++], &chipIndex, map);
        if (map[chipIndex] == BLOB_N) goto fail;

        for (int j=0; j < NUM_BLOBS; j++) {
            moveBlob(&rng, &monsterList[j], map);
        }
        if (map[chipIndex] == BLOB_N) goto fail;

        moveChip(route[i++], &chipIndex, map);
        if (map[chipIndex] == BLOB_N) goto fail;
    }
    *nummoves += i;
    printf("Successful seed: %lu, Step: %s\n", startingSeed, step == EVEN ? "even" : "odd");
    return;
fail:
    *nummoves += i;
    return;
}

static void moveChip(char dir, int *chipIndex, unsigned char map[]) {
    *chipIndex = *chipIndex + dir;
    if (map[*chipIndex] == COSMIC_CHIP) map[*chipIndex] = FLOOR;
}

// current dir => turn => new direction
static const DIR turndirs[4][4] = {
    // left, right, back
    { WEST, EAST, SOUTH,}, // NORTH
    { NORTH, SOUTH, WEST, }, // EAST
    { EAST, WEST, NORTH,}, // SOUTH
    { SOUTH, NORTH, EAST, }, // WEST
};

static const DIR xytodir[3][3] = {
    { -1, NORTH, -1 },
    { WEST, -1, EAST },
    { -1, SOUTH, -1 },
};

static void moveBlob(struct prng *rng, BLOB* b, unsigned char upper[]) {
    int dir;
    int facedir, xdir, ydir;
    do {
        xdir = randn(rng, 3);
        ydir = randn(rng, 3);
        facedir = xytodir[ydir][xdir];
    } while (facedir == -1);

    int index = b->index + diridx[facedir];
    if (canEnter(upper[index])) {
        goto ok;
    }

    unsigned int todo = 7;
    do {
        int turn = randn(rng, 3);
        if (todo & (1U<<turn)) {
            todo &= ~(1U<<turn);
            dir = turndirs[facedir][turn];
            index = b->index + diridx[dir];
            if (canEnter(upper[index])) {
                goto ok;
            }
        }
    } while (todo);
    return;
ok:
    upper[b->index] = FLOOR;
    upper[index] = BLOB_N;
    //b->dir = dir;
    b->index = index;
    return;
}

static int canEnter(unsigned char tile) {
    return (tile == FLOOR);
}

/* MSCC RNG */

static void advance(struct prng *rng)
{
    rng->value = ((rng->value * 0x343FDul) + 0x269EC3ul);
}

static int randn(struct prng *rng, int n)
{
    advance(rng);
    return (int)((rng->value>>16)&0x7fff) % n;
}
