
all: bin hashTable.o bin/hashTableTest

hashTable.o: hashTable.c hashTable.h
	gcc -Os -march=native -fno-builtin-memset hashTable.c -c -o hashTable.o -Wall -Wextra
	size hashTable.o

bin/hashTableTest: hashTableTest.c hashTable.o
	gcc -O2 -march=native hashTableTest.c -s  -o bin/hashTableTest hashTable.o -Wall -Wextra

bin:
	mkdir bin

test:
	time ./bin/hashTableTest

clean:
	rm -f hashTable.o
	rm -f bin/hashTableTest
