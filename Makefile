CC = cc
CCFLAGS = -g

default: main.o envparse.o
	$(CC) $(CCFLAGS) main.o envparse.o -o tinybuild

envparse.o: envparse.c envparse.h
	$(CC) $(CCFLAGS) -c envparse.c

main.o: main.c envparse.h util.h
	$(CC) $(CCFLAGS) -c main.c

clean:
	rm -f tinybuild *.o
