
all: bin hashTable.o

hashTable.o: hashTable.c hashTable.h
	gcc -O2 -march=native hashTable.c -c -o hashTable.o -Wall -Wextra
	size hashTable.o

bin/hashTableTest: hashTableTest.c hashTable.o
	gcc -O2 -march=native hashTableTest.c -s  -o bin/hashTableTest hashTable.o -Wall -Wextra

bin:
	mkdir bin

test: bin/hashTableTest
	time ./bin/hashTableTest

clean:
	rm -f hashTable.o
	rm -f bin/hashTableTest
