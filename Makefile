default: main.o envparse.o
	cc main.o envparse.o -o tinybuild

envparse.o: envparse.c envparse.h
	cc -c envparse.c

main.o: main.c envparse.h
	cc -c main.c

clean:
	rm -f tinybuild *.o
