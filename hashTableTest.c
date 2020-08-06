#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "hashTable.h"

typedef uint8_t  u8;
typedef int8_t   s8;
typedef uint32_t u32;
typedef int32_t  s32;
typedef uint64_t u64;
typedef int64_t  s64;
typedef float    f32;
typedef double   f64;

#define UPPER_LIMIT 1000000

//#define PRINTOUT

int main(void)
{
	HashTable *ht;
	hashTableNode *node;
	char buff[128];
	s64 res=0;
	s32 returnCode;
	
	printf("Start of Test:\n");
	returnCode=hashTable_init(&ht);
	if(returnCode){
		printf("hashTable_init: %s\n", hashTable_debugString(returnCode));
	}
	for (s64 x=1; x<=UPPER_LIMIT; x++){
		sprintf(buff, "%ld", x);
		if ( hashTable_insert(ht, (u8*)buff, strlen(buff), 0) ){
			printf("Strange failure to insert %ld\n", x);
		}
	}
	
	res = hashTable_countEachNode(ht);
	printf("hashTable_countEachNode is %ld\n", res);
	printf("ht->count is %d\n", ht->count);
	res = hashTable_maxChain(ht);
	printf("hashTable_maxDepth is %ld\n", res);
	
	for (s64 x=1; x<=UPPER_LIMIT; x++){
		sprintf(buff, "%ld", x);
		hashTable_find(ht, (u8*)buff, strlen(buff), &node);
		#ifdef PRINTOUT
		printf("found node %s\n", node->key);
		#endif
		if (hashTable_delete(ht, (u8*)buff, strlen(buff), 0) ){
			printf("Strange failure to delete %ld\n", x);
		}
	}
	
	res = hashTable_countEachNode(ht);
	printf("hashTable_countEachNode is %ld\n", res);
	printf("ht->count is %d\n", ht->count);
	
	for (s64 x=1; x<=UPPER_LIMIT; x++){
		if(hashTable_insertIntKey(ht, x, 0)){
			printf("Strange failure to insert %ld\n", x);
		}
	}
	
	res = hashTable_countEachNode(ht);
	printf("hashTable_countEachNode is %ld\n", res);
	printf("ht->count is %d\n", ht->count);
	res = hashTable_maxChain(ht);
	printf("hashTable_maxDepth is %ld\n", res);
	
	for (s64 x=1; x<=UPPER_LIMIT; x++){
		hashTable_findIntKey(ht, x, &node);
		#ifdef PRINTOUT
		printf("found node %ld\n", hashTable_stringTos64(node->key));
		#endif
		hashTable_deleteIntKey(ht, x, 0);
	}
	
	res = hashTable_countEachNode(ht);
	printf("hashTable_countEachNode is %ld\n", res);
	printf("ht->count is %d\n", ht->count);
	
	printf("calling free all\n");
	hashTable_freeAll(&ht);
	
	res = hashTable_countEachNode(ht);
	printf("hashTable_countEachNode is %ld\n", res);

	return 0;
}

