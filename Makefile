Blob: *.c Makefile
	gcc -ggdb Search.c -lpthread -Wall -Wextra -O2 -o Blob

Blob.exe: *.c Makefile
	i686-w64-mingw32-gcc -static -ggdb Search.c -lpthread -Wall -Wextra -O2 -o Blob.exe
