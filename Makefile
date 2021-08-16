CC = cc
CCFLAGS = -Wall

default: main.o envparse.o namespaces.o util.o
	$(CC) $(CCFLAGS) main.o envparse.o namespaces.o util.o -o tinybuild

envparse.o: envparse.c envparse.h
	$(CC) $(CCFLAGS) -c envparse.c

main.o: main.c envparse.h namespaces.h util.h
	$(CC) $(CCFLAGS) -c main.c

namespaces.o: namespaces.c namespaces.h
	$(CC) $(CCFLAGS) -c namespaces.c

util.o: util.c util.h
	$(CC) $(CCFLAGS) -c util.c

clean:
	rm -f tinybuild *.o
