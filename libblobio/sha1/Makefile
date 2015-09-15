all: sha1.o
clean:
	rm -f sha1.o
sha1.o: sha1.c
	gcc -c -O4 -I.. sha1.c
	ar rcs ../libblobio.a sha1.o
