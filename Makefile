CC = cc
CCFLAGS = -Wall

default: build/main.o build/envparse.o build/namespaces.o build/util.o build/md5.o
	$(CC) $(CCFLAGS) build/main.o build/envparse.o build/namespaces.o build/util.o build/md5.o -o build/tinybuild

build/envparse.o: src/envparse.c src/envparse.h
	$(CC) $(CCFLAGS) -c src/envparse.c -o build/envparse.o

build/main.o: src/main.c src/envparse.h src/namespaces.h src/util.h
	$(CC) $(CCFLAGS) -c src/main.c -o build/main.o

build/namespaces.o: src/namespaces.c src/namespaces.h
	$(CC) $(CCFLAGS) -c src/namespaces.c -o build/namespaces.o

build/util.o: src/util.c src/util.h
	$(CC) $(CCFLAGS) -c src/util.c -o build/util.o

build/md5.o: src/md5/md5.c
	$(CC) $(CCFLAGS) -c src/md5/md5.c -o build/md5.o

clean:
	rm -f build/tinybuild build/*.o
