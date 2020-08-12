/* hashTable.h */

#ifndef HASHTABLE_HEADER
#define HASHTABLE_HEADER
#include <stdint.h>

/*******************************************************************************
 * Section Interface for Alterations
*******************************************************************************/
#define HASHTABLE_DEBUG

#ifndef HASHTABLE_CUSTOM_ALLOC
#include <stdlib.h>
#define HASHTABLE_FREE   free
#define HASHTABLE_MALLOC malloc
#define HASHTABLE_CALLOC calloc
#endif

#ifndef HASHTABLE_CUSTOM_TYPE
typedef uint64_t HtValue;
#endif

// compare needs to be of type
// int32_t yourCompareFunction(uint8_t *s1, uint8_t *s1)
#ifndef HASHTABLE_CUSTOM_CMP
#define HT_CMP(x,y)  (stringCompare((x),(y)))
#endif

// hash needs to be of type
// uint64_t yourHashFunction(uint8_t *key, uint8_t keyLen, uint64_t seed)
#ifndef HASHTABLE_CUSTOM_HASH
#define HT_HASH(x,y,z)  (computeHash((x),(y),(z)))
#endif

#ifdef HASHTABLE_STATIC_BUILD_IN
#define HASHTABLE_STATIC_BUILD static
#else
#define HASHTABLE_STATIC_BUILD
#endif

#define BASE_SIZE (64)

/*******************************************************************************
 * Section Types
*******************************************************************************/

typedef struct HtNode_s hashTableNode;

typedef struct HtNode_s {
	hashTableNode *next;
	HtValue       value;
	uint32_t      hash;
	uint8_t       keyLen;
	uint8_t       key[3];
} hashTableNode;

typedef struct {
	hashTableNode **table;
	uint64_t      seed;
	uint32_t      count;
	uint32_t      size;
} HashTable;

// Main Function API error enumeration
enum {
	// errors
	hashTable_errorNullParam1         = -1,
	hashTable_errorNullParam2         = -2,
	hashTable_errorNullParam3         = -3,
	hashTable_errorTableIsEmpty       = -4,
	hashTable_errorMallocFailed       = -5,
	// worked as expected
	hashTable_OK                      =  0,
	// not an error, but did not work as expected
	hashTable_nothingFound            =  1,
	hashTable_updatedValOfExistingKey =  2,
};

/*******************************************************************************
 * Section Main Function API
 * Return values are of the enumeration in Types
*******************************************************************************/

HASHTABLE_STATIC_BUILD
int32_t
hashTable_init(HashTable **ht_p);

HASHTABLE_STATIC_BUILD
int32_t
hashTable_insert(
	HashTable *ht,    // pointer memory holding address of tree
	uint8_t   *key,   // pointer to c-string key(null required)
	uint8_t   keyLen, // length of key in bytes(not including null)
	HtValue   value); // value to be stored

// convenience function for using integer keys
HASHTABLE_STATIC_BUILD
int32_t
hashTable_insertIntKey(
	HashTable *ht,    // pointer memory holding address of tree
	int64_t   key,    // signed integer key
	HtValue   value); // value to be stored

HASHTABLE_STATIC_BUILD
int32_t
hashTable_find(
	HashTable     *ht,       // pointer to hash table
	uint8_t       *key,      // pointer to c-string key(null required)
	uint8_t       keyLen,    // length of key in bytes(not including null)
	hashTableNode **result); // address for search result to be written

// convenience function for using integer keys
HASHTABLE_STATIC_BUILD
int32_t
hashTable_findIntKey(
	HashTable     *ht,       // pointer to tree
	int64_t       key,       // signed integer key
	hashTableNode **result); // address for search result to be written

HASHTABLE_STATIC_BUILD
int32_t
hashTable_delete(
	HashTable *ht,     // pointer memory holding address of tree
	uint8_t   *key,    // pointer to c-string key(null required)
	uint8_t   keyLen,  // length of key in bytes(not including null)
	HtValue   *value); // OPTIONAL: pointer to memory for value to written

// convenience function for using integer keys
HASHTABLE_STATIC_BUILD
int32_t
hashTable_deleteIntKey(
	HashTable *ht,   // pointer memory holding address of tree
	int64_t key,     // signed integer key
	HtValue *value); // OPTIONAL: pointer to memory for value to written

/*******************************************************************************
 * Section Helper/Utility Function API
*******************************************************************************/

#ifdef HASHTABLE_DEBUG
HASHTABLE_STATIC_BUILD
uint8_t*
hashTable_debugString(int32_t mainAPIReturnValue);
#endif

HASHTABLE_STATIC_BUILD
uint64_t
hashTable_getSeed(HashTable *ht);

HASHTABLE_STATIC_BUILD
void
hashTable_setSeed(HashTable *ht, uint64_t seed);

HASHTABLE_STATIC_BUILD
uint32_t
hashTable_countEachNode(HashTable *ht);

HASHTABLE_STATIC_BUILD
uint32_t
hashTable_maxChain(HashTable *ht);

HASHTABLE_STATIC_BUILD
void
hashTable_freeAll(HashTable **ht_p);

HASHTABLE_STATIC_BUILD
uint32_t
hashTable_s64toString(int64_t input, uint8_t *output);

HASHTABLE_STATIC_BUILD
int64_t 
hashTable_stringTos64(uint8_t *string);

/*******************************************************************************
 * Section Inline traversal MACRO API
 * This can be used to implement function programming style function or
 * iterators. Such as map, filter, reduce, etc.
 * 
 * Parameter explanation:
 * ht - pointer to ht.
 * 
 * function - function name that takes a pointer to a tree as a first parameter
 * and "parameter" of what ever type as second parameter.
 * 
 * function return value is zero to signal OK to continue or non-zero to exit
 * iteration early. You can use this to execute over a range, etc.
 * 
 * parameter - this is a paramater of your choosing, for example to store state
 * or to pass data to your function.
 * 
*******************************************************************************/

#define HASHTABLE_TRAVERSAL(ht, function, parameter) \
do{ \
	if(ht){ \
		__label__ EXIT; \
		hashTableNode **table = ht->table; \
		hashTableNode *cur_node; \
		u32 size; \
		size = ht->size; \
		for(u32 x = 0; x < size; x++) \
		{ \
			cur_node = table[x]; \
			while(cur_node){ \
				if(function(cur_node, parameter)){ \
					goto EXIT; \
				} \
				cur_node = cur_node->next; \
			} \
		} \
	} \
	EXIT: ;\
}while(0)

#endif
