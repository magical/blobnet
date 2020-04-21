Blob: *.c Makefile
	gcc -O2 -ggdb -fopenmp -Wall -Wextra -O2 -o Blob Search.c

Blob.exe: *.c Makefile
	i686-w64-mingw32-gcc -O2 -ggdb -fopenmp -Wall -Wextra -static -o Blob.exe Search.c
