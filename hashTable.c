/* hashTable.c */

#include "hashTable.h"

typedef uint8_t  u8;
typedef int8_t   s8;
typedef uint32_t u32;
typedef int32_t  s32;
typedef uint64_t u64;
typedef int64_t  s64;

#define ENTRY_SIZE (sizeof(hashTableNode*))

/*******************************************************************************
 * Section Internal Functions
 ******************************************************************************/

static inline s32
stringCompare(u8 *str1, u8 *str2) __attribute__((always_inline));

static inline s32
stringCompare(u8 *str1, u8 *str2)
{
	s32 c1, c2;
	
	while(1){
		c1=*str1;
		str1+=1;
		c2=*str2;
		str2+=1;
		c1-=c2;
		if( (c1!=0) || (c2==0) ){
			return c1;
		}
	}
}

static inline u32
getNodeSize(u32 keyLen)
{
	// return nodeLen in bytes. Assume null termination, add 1
	return (keyLen+1+sizeof(HtValue)+17+7)/8*8; // round up to 8 bytes
}

static inline u32
getMask(u32 size)
{
	return size-1;
}

static inline hashTableNode *
makeNode(HashTable *ht, u8 *key, u32 keyLen, HtValue value, u64 hash)
{
	u32 i=0, nodeSize;
	hashTableNode *new;
	nodeSize = getNodeSize(keyLen);
	
	new = HASHTABLE_MALLOC(nodeSize);
	
	if(new){
		ht->count++;
		new->next = 0;
		new->value = value;
		new->hash = hash;
		new->keyLen = keyLen;
		do{new->key[i]=key[i];i++;}while(i<keyLen);
		new->key[keyLen] = 0; // null terminate
	}
	return new;
}

static inline s64 
keyCmp(
	u8 *key1,
	u32 key1Len,
	u64 key1Hash,
	u8 *key2,
	u32 key2Len,
	u64 key2Hash)
{
	s64 res;
	
	res = (key1Len-key2Len)|(key1Hash-key2Hash);
	if(res == 0){
		res = HT_CMP(key1,key2);
	}
	
	return res;
}

// slightly modified fnv-1 algorithm, public domain
static inline u64 
computeHash(u8 *key, u8 keyLen, u64 seed)
{
	u64 hash = seed;
	u32 x=0;

	while(1)
	{
		hash = (key[x]) + (hash * 0x00000100000001B3);
		x++;
		if(x>=keyLen) {
			break;
		}
	}

	return hash;
}

static void
insert_node(
	hashTableNode *n,
	u32 mask,
	hashTableNode **table)
{
	u64 hash;
	hashTableNode *curNode;
	hash = n->hash & mask;
	curNode = table[hash];
	n->next = curNode;
	table[hash] = n;
}

static void
insertInToNewTable(HashTable *ht, u32 size, u32 mask, hashTableNode **oldTable)
{
	hashTableNode *curNode, *nextNode;
	hashTableNode **table = ht->table;
	u32 x = 0;
	do {
		curNode = oldTable[x];
		while(curNode){
			// there is atleast one thing here
			nextNode=curNode->next;
			insert_node(curNode, mask, table);
			curNode=nextNode;
		}
		x++;
	} while (x < size);
}

static s32
newTableAndPopulate(HashTable *ht, u32 oldSize, u64 newSize)
{
	hashTableNode **oldTable;
	u32 mask;
	// save off old table
	oldTable = ht->table;
	// make new table
	ht->table = HASHTABLE_CALLOC(1, newSize*ENTRY_SIZE);
	
	if (ht->table==0)
	{
		// set table back to old one and report error
		ht->table = oldTable;
		return hashTable_errorCannotMakeNewTable;
	}
	
	// put everything into new table
	mask = getMask(newSize);
	insertInToNewTable(ht, oldSize, mask, oldTable);
	// free old tree
	HASHTABLE_FREE(oldTable);
	return hashTable_OK;
}

static s32
checkSizeToGrow(HashTable *ht)
{
	u32 oldSize;
	u64 newSize;
	oldSize = ht->size;
	// check if we need to re-size hashtable
	if( (ht->count+1) <= (oldSize) )
	{
		return hashTable_OK;
	}
	// resize to double, new table will be half full
	newSize = oldSize*2;
	ht->size = newSize;
	
	return newTableAndPopulate(ht, oldSize, newSize);
}

static s32
checkSizeToShrink(HashTable *ht)
{
	u32 oldSize;
	u64 newSize;
	oldSize = ht->size;
	// check for minimum hash table size
	if (oldSize <= 8)
	{
		return hashTable_OK;
	}
	// check if we need to re-size hashtable
	if( (ht->count-1) >= (oldSize/4) )
	{
		return hashTable_OK;
	}
	// resize to half, new table will be half full
	newSize = oldSize/2;
	ht->size = newSize;
	
	return newTableAndPopulate(ht, oldSize, newSize);
}

/*******************************************************************************
 * Section Init
*******************************************************************************/

HASHTABLE_STATIC_BUILD
s32
hashTable_init(HashTable **ht_p)
{
	HashTable *ht;
	if(ht_p==0){
		return hashTable_errorNullParam1;
	}
	ht = HASHTABLE_MALLOC(sizeof(HashTable));
	if(ht==0){
		return hashTable_errorMallocFailed;
	}
	ht->table = HASHTABLE_CALLOC(1, BASE_SIZE);
	if(ht->table==0){
		return hashTable_errorMallocFailed;
	}
	ht->seed =  0xcbf29ce484222325;
	ht->count = 0;
	ht->size  = BASE_SIZE/ENTRY_SIZE;
	*ht_p = ht;
	return hashTable_OK;
}

/*******************************************************************************
 * Section Insertion
 ******************************************************************************/

static s32
HashTable_insert_internal(
	HashTable *ht,
	u8        *key,
	u8         keyLen,
	HtValue    value)
{
	u64 hash, maskedHash;
	hashTableNode *newNode, *curNode, **nodeAddr;
	s32 returnCode;
	
	returnCode = checkSizeToGrow(ht);
	
	hash = HT_HASH(key, keyLen, ht->seed);
	// search for existing key
	maskedHash = hash & getMask(ht->size);
	nodeAddr = &ht->table[maskedHash];

	// begin search for slot
	while (1) {
		curNode = *nodeAddr;
		if (curNode == 0)
		{
			// nothing exists, make node and insert
			newNode = makeNode(ht, key, keyLen, value, hash);
			if (newNode==0) {
				return hashTable_errorMallocFailed;
			}
			*nodeAddr = newNode;
			return returnCode;
		}
		if (keyCmp(
				key,
				keyLen,
				hash,
				curNode->key,
				curNode->keyLen,
				curNode->hash)==0 ) {
			// key does exist, update value
			curNode->value = value;
			return hashTable_updatedValOfExistingKey;
		}
		nodeAddr = &curNode->next;
	}
}

HASHTABLE_STATIC_BUILD
s32
hashTable_insert(
	HashTable *ht,
	u8        *key,
	u8        keyLen,
	HtValue   value)
{
	if(ht==0){
		return hashTable_errorNullParam1;
	}
	if(key==0){
		return hashTable_errorNullParam2;
	}
	if(keyLen==0){
		return hashTable_errorNullParam3;
	}
	return HashTable_insert_internal(ht, key, keyLen, value);
}

HASHTABLE_STATIC_BUILD
s32
hashTable_insertIntKey(
	HashTable *ht,
	s64       key,
	HtValue   value)
{
	u8 keyBuffer[16];
	u8 keyLen;
	if(ht==0){
		return hashTable_errorNullParam1;
	}
	keyLen = hashTable_s64toString(key, keyBuffer);
	return HashTable_insert_internal(ht, keyBuffer, keyLen, value);
}

/*******************************************************************************
 * Section Find
*******************************************************************************/

static hashTableNode *
hashTable_find_internal(
	HashTable *ht,
	u8        *key,
	u8        keyLen)
{
	u64 hash, maskedHash;
	hashTableNode *curNode;
	
	hash = HT_HASH(key, keyLen, ht->seed);
	// search for existing key
	maskedHash = hash & getMask(ht->size);
	curNode = ht->table[maskedHash];
	
	// something exists,  might be key
	while (1) {
		if (curNode == 0)
		{
			// nothing exists
			return 0;
		}
		if(keyCmp(
			key,
			keyLen,
			hash,
			curNode->key,
			curNode->keyLen,
			curNode->hash)==0){
			// key does exist, return key
			return curNode;
		}
		curNode=curNode->next;
	}
}

HASHTABLE_STATIC_BUILD
s32
hashTable_find(
	HashTable      *ht,
	u8             *key,
	u8              keyLen,
	hashTableNode **result)
{
	hashTableNode *internalResult;
	if(ht==0){
		return hashTable_errorNullParam1;
	}
	if(key==0){
		return hashTable_errorNullParam2;
	}
	if(keyLen==0){
		return hashTable_errorNullParam3;
	}
	if(result==0){
		return hashTable_errorNullParam4;
	}
	
	internalResult = hashTable_find_internal(ht, key, keyLen);
	if(internalResult==0){
		return hashTable_nothingFound;
	}
	
	*result = internalResult;
	return hashTable_OK;
}



HASHTABLE_STATIC_BUILD
s32
hashTable_findIntKey(
	HashTable     *ht,
	s64           key,
	hashTableNode **result)
{
	u8 keyBuffer[16];
	u8 keyLen = hashTable_s64toString(key, keyBuffer);
	return hashTable_find(ht, keyBuffer, keyLen, result);
}

/*******************************************************************************
 * Section Deletion
*******************************************************************************/

static s32
hashTable_delete_internal(
	HashTable *ht,
	u8        *key,
	u8        keyLen,
	HtValue   *value)
{
	u64 hash, maskedHash;
	hashTableNode **curSlotAddr, *node;
	s32 returnCode;
	
	returnCode = checkSizeToShrink(ht);
	
	hash = HT_HASH(key, keyLen, ht->seed);
	// search for existing key
	maskedHash = hash & getMask(ht->size);
	curSlotAddr = &ht->table[maskedHash];
	
	// something exists,  might be key
	while (1){
		node = *curSlotAddr;
		if (node == 0)
		{
			// nothing exists
			return hashTable_nothingFound;
		}
		if(keyCmp(
			key,
			keyLen,
			hash,
			node->key,
			node->keyLen,
			node->hash)==0){
			// key does exist, delete key
			// if passed a pointer write out value
			if(value){
				*value = node->value;
			}
			// over write memory with next address
			*curSlotAddr = node->next;

			HASHTABLE_FREE(node);
			ht->count--;
			return returnCode;
		}
		curSlotAddr = &node->next;
	}
}

HASHTABLE_STATIC_BUILD
s32
hashTable_delete(
	HashTable *ht,
	u8        *key,
	u8        keyLen,
	HtValue   *value)
{	
	if(ht==0){
		return hashTable_errorNullParam1;
	}
	if(key==0){
		return hashTable_errorNullParam2;
	}
	if(keyLen==0){
		return hashTable_errorNullParam3;
	}
	return hashTable_delete_internal(ht, key, keyLen, value);
}

HASHTABLE_STATIC_BUILD
s32
hashTable_deleteIntKey(
	HashTable *ht,
	s64       key,
	HtValue   *value)
{
	u8 keyBuffer[16];
	u8 keyLen;
	if(ht==0){
		return hashTable_errorNullParam1;
	}
	keyLen = hashTable_s64toString(key, keyBuffer);
	return hashTable_delete_internal(ht, keyBuffer, keyLen, value);
}

/*******************************************************************************
 * Section Helper Functions
*******************************************************************************/

HASHTABLE_STATIC_BUILD
u64
hashTable_getSeed(HashTable *ht)
{
	return ht->seed;
}

HASHTABLE_STATIC_BUILD
void
hashTable_setSeed(HashTable *ht, u64 seed)
{
	ht->seed = seed;
}

HASHTABLE_STATIC_BUILD
u32
hashTable_getCount(HashTable *ht)
{
	return ht->count;
}

/*******************************************************************************
 * Section Utilities
*******************************************************************************/

HASHTABLE_STATIC_BUILD
u32
hashTable_maxChain(HashTable *ht)
{
	hashTableNode **table;
	u32 chainCount, size, max, x;
	hashTableNode *curNode;
	if(ht==0){
		return 0;
	}
	max = 0;
	table = ht->table;
	size = ht->size;
	for(x = 0; x < size; x++)
	{
		curNode = table[x];
		chainCount = 0;
		while(curNode){
			// there is atleast one thing here
			chainCount++;
			curNode=curNode->next;
		}
		if(chainCount>max){
			max = chainCount;
		}
	}

	return max;
}

HASHTABLE_STATIC_BUILD
u32
hashTable_countEachNode(HashTable *ht)
{
	hashTableNode **table;
	hashTableNode *curNode;
	u32 count, size, x;
	if(ht==0){
		return 0;
	}
	count = 0;
	table = ht->table;
	size = ht->size;
	for(x = 0; x < size; x++)
	{
		curNode = table[x];
		while(curNode){
			// there is atleast one thing here
			count++;
			curNode = curNode->next;
		}
	}
	return count;
}

HASHTABLE_STATIC_BUILD
void
hashTable_freeAll(HashTable **ht_p)
{
	HashTable *ht;
	hashTableNode **table;
	hashTableNode *curNode, *prevNode;
	u32 size, x;
	if (ht_p==0) {
		return;
	}
	ht = *ht_p;
	*ht_p = 0;
	table = ht->table;
	size = ht->size;
	for(x = 0; x < size; x++)
	{
		curNode = table[x];
		while(curNode){
			// there is atleast one thing here
			prevNode = curNode;
			curNode = curNode->next;
			HASHTABLE_FREE(prevNode);
		}
	}
	HASHTABLE_FREE(table);
	HASHTABLE_FREE(ht);
}

#define SIGN_MASK   0x8000000000000000
#define UNSIGN_MASK 0x7FFFFFFFFFFFFFFF

HASHTABLE_STATIC_BUILD
u32
hashTable_s64toString(s64 signedInput, u8 *output)
{
	u8  tempOutput[16];
	u8  *outputp = output;
	s64 count = 0;
	u64 tmp;
	u64 input = signedInput;
	u8  header;
	u8  isPositive = ((input&SIGN_MASK)==0);
	// null terminate
	tempOutput[count] = '\0';
	// take off sign bit
	input = input & UNSIGN_MASK;
	// range 1 - 64
	header = 65 - __builtin_clzl(input); 
	// set high bit on positive number's first byte (Big Endian)
	header = header | (isPositive<<7);
	
	do{
		tmp = input % 128;
		input = input / 128;
		count++;
		tempOutput[count] = (tmp+1);
	}while(input);
	
	count++;
	tempOutput[count] = header;
	
	do{
		*outputp = tempOutput[count];
		outputp++;
		count--;
	}while (count);
	
	*outputp = tempOutput[0];
	return (outputp - output);
}

HASHTABLE_STATIC_BUILD
s64
hashTable_stringTos64(u8 *string)
{
	u64 val = 0;
	u32 stringVal;
	u64 isNegative = ((*string)&0x80);
	isNegative<<=56;
	
	// skip header
	++string;
	
	while ( (*string >= 1) && (*string <= 128) ) {
		stringVal = (*string) - 1;
		val = (val * 128) + stringVal;
		++string;
	}
	
	// add back in sign bit
	val = val | isNegative;
	
	return (s64)val;
}

/*******************************************************************************
 * Section Debug
*******************************************************************************/

#ifdef HASHTABLE_DEBUG
HASHTABLE_STATIC_BUILD
u8 *
hashTable_debugString(s32 mainAPIReturnValue)
{
	switch(mainAPIReturnValue){
		case hashTable_errorNullParam1:
		return (u8*)"hashTable Error: First parameter provided is NULL(0).\n";
		case hashTable_errorNullParam2:
		return (u8*)"hashTable Error: Second parameter provided is NULL(0).\n";
		case hashTable_errorNullParam3:
		return (u8*)"hashTable Error: Third parameter provided is NULL(0).\n";
		case hashTable_errorNullParam4:
		return (u8*)"hashTable Error: Forth parameter provided is NULL(0).\n";
		case hashTable_errorMallocFailed:
		return (u8*)"hashTable Error: "
					"Malloc was called and returned NULL(0).\n";
		case hashTable_errorCannotMakeNewTable:
		return (u8*)"hashTable Error: "
					"Calloc was called and returned NULL(0). Cannot make "
					"new table, using old table (capacity above 1.0).\n";
		case hashTable_OK:
		return (u8*)"hashTable OK: Everything worked as intended.\n";
		case hashTable_nothingFound:
		return (u8*)"hashTable Status: "
					"Nothing Found. Search for node terminated"
					" with nothing in find or delete.\n";
		case hashTable_updatedValOfExistingKey:
		return (u8*)"hashTable Status: "
					"Existing key found and value updated.\n";
		default:
		return (u8*)"hashTable Default: This value is not enumerated."
		       " Debug has no information for you.\n";
	}
}
#endif
