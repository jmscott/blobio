all: 
	cd Optimized_32bit && $(MAKE) install

clean:
	cd Optimized_32bit && $(MAKE) clean
	rm -f *.h

libskein.a: nab.o
	ar rcs libskein.a nab.o
skein.h:
	cd Optimized_32bit && $(MAKE) install
