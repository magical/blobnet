Blob: *.c blobnet.o Makefile
	gcc -O2 -ggdb -fopenmp -Wall -Wextra -O2 -o Blob Search.c blobnet.o

Blob.exe: *.c blobnet_w32.o Makefile
	i686-w64-mingw32-gcc -O2 -ggdb -fopenmp -Wall -Wextra -static -o Blob.exe Search.c blobnet_w32.o

blobnet.o: blobnet.bin
	$(LD) -r -b binary -o $@ $<

blobnet_w32.o: blobnet.bin
	i686-w64-mingw32-ld -r -b binary -o $@ $<
