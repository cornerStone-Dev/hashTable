/* hashTable.c */

#include "hashTable.h"

typedef uint8_t  u8;
typedef int8_t   s8;
typedef uint32_t u32;
typedef int32_t  s32;
typedef uint64_t u64;
typedef int64_t  s64;

#define ENTRY_SIZE (sizeof(hashTableNode*))
#define MASK_32_BITS 0xFFFFFFFF

/*******************************************************************************
 * Section Local Prototypes
 ******************************************************************************/

static inline s32
stringCompare(u8 *str1, u8 *str2) __attribute__((always_inline));

static hashTableNode * 
makeNode(HashTable *ht, u8 *key, u32 keyLen, u64 val, u32 hashVal);

static inline u32
getNodeLen(u32 keyLen);

static inline u32
getMask(u32 size);

static s32
checkForSpace(HashTable *ht);

static inline hashTableNode * 
makeNode(HashTable *ht, u8 *key, u32 keyLen, HtValue value, u32 hashVal);

static inline s32 
keyCmp(
	u8 *key1,
	u32 key1Len,
	u32 key1Hash,
	u8 *key2,
	u32 key2Len,
	u32 key2Hash);

static inline u64 
computeHash(u8 *key, u8 keyLen, u64 seed);

/*******************************************************************************
 * Section Init
*******************************************************************************/

HASHTABLE_STATIC_BUILD
s32
hashTable_init(HashTable **ht_p)
{
	if(ht_p==0){
		return hashTable_errorNullParam1;
	}
	HashTable *ht = HASHTABLE_MALLOC(sizeof(HashTable));
	if(ht==0){
		return hashTable_errorMallocFailed;
	}
	ht->table = HASHTABLE_MALLOC(BASE_SIZE);
	if(ht->table==0){
		return hashTable_errorMallocFailed;
	}
	HASHTABLE_MEMSET(ht->table, 0, BASE_SIZE);
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
	u8        keyLen,
	HtValue   value);

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
	return HashTable_insert_internal(ht, key, keyLen, value);
}

static s32
HashTable_insert_internal(
	HashTable *ht,
	u8        *key,
	u8        keyLen,
	HtValue   value)
{
	u64 hash;
	u32 hashVal;
	s32 returnCode;
	hashTableNode *new_node, *cur_node;
	
	returnCode = checkForSpace(ht);
	if(returnCode){return returnCode;}
	
	hash = HT_HASH(key, keyLen, ht->seed);
	// search for existing key
	hashVal = hash & MASK_32_BITS;
	hash = hash & getMask(ht->size);
	cur_node = ht->table[hash];
	if(cur_node){
		// something exists,  might be key
		while (1){
			if(keyCmp(
				key,
				keyLen,
				hashVal,
				cur_node->key,
				cur_node->keyLen,
				cur_node->hash)==0){
				// key does exist, update value
				cur_node->value = value;
				return hashTable_updatedValOfExistingKey;
			}
			if(cur_node->next==0){
				// key does not exist, but shares hash value
				new_node = makeNode(ht, key, keyLen, value, hashVal);
				if(new_node==0){
					return hashTable_errorMallocFailed;
				}
				cur_node->next = new_node;
				return hashTable_OK;
			}
			cur_node=cur_node->next;
		}
	} else {
		// nothing exists, make node and insert
		new_node = makeNode(ht, key, keyLen, value, hashVal);
		if(new_node==0){
			return hashTable_errorMallocFailed;
		}
		ht->table[hash] = new_node;
		return hashTable_OK;
	}
}

HASHTABLE_STATIC_BUILD
s32
hashTable_insertIntKey(
	HashTable *ht,
	s64       key,
	HtValue   value)
{
	u8 keyBuffer[16];
	u8 keyLen = hashTable_s64toString(key, keyBuffer);
	return hashTable_insert(
				ht,
				keyBuffer,
				keyLen,
				value);
}

static void
hashTable_insert_node(HashTable *ht, hashTableNode *n, u32 mask)
{
	u64 hash;
	hashTableNode *cur_node;
	hash = n->hash & mask;
	cur_node = ht->table[hash];
	if(cur_node){
		// something exists, add to hash chain
		n->next = cur_node;
		ht->table[hash] = n;
	} else {
		// nothing exists, insert node
		ht->table[hash] = n;
	}
}

/*******************************************************************************
 * Section Find
*******************************************************************************/

static hashTableNode *
hashTable_find_internal(
	HashTable *ht,
	u8        *key,
	u8        keyLen);

HASHTABLE_STATIC_BUILD
s32
hashTable_find(
	HashTable *ht,
	u8        *key,
	u8        keyLen,
	hashTableNode **result)
{
	hashTableNode *internalResult;
	if(ht==0){
		return hashTable_errorNullParam1;
	}
	if(key==0){
		return hashTable_errorNullParam2;
	}
	if(result==0){
		return hashTable_errorNullParam3;
	}
	if(ht->count==0){
		return hashTable_errorTableIsEmpty;
	}
	internalResult = hashTable_find_internal(ht, key, keyLen);
	
	if(internalResult==0){
		return hashTable_nothingFound;
	}
	
	*result = internalResult;
	return hashTable_OK;
}

static hashTableNode *
hashTable_find_internal(
	HashTable *ht,
	u8        *key,
	u8        keyLen)
{
	u64 hash;
	u32 hashVal;
	hashTableNode *cur_node;
	
	hash = HT_HASH(key, keyLen, ht->seed);
	// search for existing key
	hashVal = hash & MASK_32_BITS;
	hash = hash & getMask(ht->size);
	cur_node = ht->table[hash];
	if(cur_node){
		// something exists,  might be key
		while (1){
			if(keyCmp(
				key,
				keyLen,
				hashVal,
				cur_node->key,
				cur_node->keyLen,
				cur_node->hash)==0){
				// key does exist, return key
				return cur_node;
			}
			if(cur_node->next==0){
				// key does not exist, but shares hash value
				return 0;
			}
			cur_node=cur_node->next;
		}
	} else {
		// nothing exists
		return 0;
	}
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
	u64 hash;
	u32 hashVal;
	hashTableNode *cur_node, *prev_node=0;
	
	hash = HT_HASH(key, keyLen, ht->seed);
	// search for existing key
	hashVal = hash & MASK_32_BITS;
	hash = hash & getMask(ht->size);
	cur_node = ht->table[hash];
	if(cur_node){
		// something exists,  might be key
		while (1){
			if(keyCmp(
				key,
				keyLen,
				hashVal,
				cur_node->key,
				cur_node->keyLen,
				cur_node->hash)==0){
				// key does exist, delete key
				if(value){
					*value = cur_node->value;
				}
				// relink list if applicable
				if((cur_node->next==0)&&(prev_node==0)){
					// single entry
					ht->table[hash]=0;
				} else
				if((cur_node->next!=0)&&(prev_node==0)){
					// start of chain
					ht->table[hash]=cur_node->next;
				} else
				if((cur_node->next!=0)&&(prev_node!=0)){
					// middle of chain
					prev_node->next=cur_node->next;
				} else {
					// end entry
					prev_node->next=0;
				}
				free(cur_node);
				ht->count--;
				return hashTable_OK;
			}
			if(cur_node->next==0){
				// key does not exist, but shares hash value
				return hashTable_nothingFound;
			}
			prev_node = cur_node;
			cur_node=cur_node->next;
		}
	} else {
		// nothing exists
		return hashTable_nothingFound;
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
	if(ht->count==0){
		return hashTable_errorTableIsEmpty;
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
	u8 keyLen = hashTable_s64toString(key, keyBuffer);
	return hashTable_delete(ht, keyBuffer, keyLen, value);
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

// slightly modified fnv-1 algorithm, public domain
static inline u64 
computeHash(u8 *key, u8 keyLen, u64 seed)
{
	u64 hash = seed;

	for (u32 x=0; x<keyLen; x++)
	{
		hash = (key[x]) + (hash * 0x00000100000001B3);
	}

	return hash;
}

static inline u32
getMask(u32 size)
{
	return size-1;
}

static inline u32
getNodeLen(u32 keyLen)
{
	// return nodeLen in bytes. Assume null termination, add 1
	return (keyLen+1+sizeof(HtValue)+13+7)/8*8; // round up to 8 bytes
}

static s32
checkForSpace(HashTable *ht)
{
	hashTableNode **old_table;
	u32 size, htSize, mask;
	// check if we need to re-size hashtable
	if( (ht->count+1) <= (ht->size) )
	{
		return hashTable_OK;
	}
	ht->size*=2;
	htSize = ht->size;
	// save off old table
	old_table = ht->table;
	// make new table
	ht->table = HASHTABLE_MALLOC(htSize*ENTRY_SIZE);
	if(ht->table){
		HASHTABLE_MEMSET(ht->table, 0, htSize*ENTRY_SIZE);
		// re-hash(if needed) everything and insert
		hashTableNode *cur_node, *next_node;
		size = (htSize/2);
		mask = getMask(htSize); 
		for(u32 x = 0; x < size; x++)
		{
			cur_node = old_table[x];
			while(cur_node){
				// there is atleast one thing here
				next_node=cur_node->next;
				cur_node->next = 0;
				hashTable_insert_node(ht, cur_node, mask);
				cur_node=next_node;
			}
		}
		// free old tree
		free(old_table);
		return hashTable_OK;
	} else {
		// set table back to old one and report error
		ht->table = old_table;
		return hashTable_errorMallocFailed;
	}
}


static inline hashTableNode *
makeNode(HashTable *ht, u8 *key, u32 keyLen, HtValue value, u32 hashVal)
{
	u32 i=0, nodeSize;
	nodeSize = getNodeLen(keyLen);
	
	hashTableNode *new = HASHTABLE_MALLOC(nodeSize);
	
	if(new){
		ht->count++;
		new->next = 0;
		new->value = value;
		new->hash = hashVal;
		new->keyLen = keyLen;
		do{new->key[i]=key[i];i++;}while(i<keyLen);
		new->key[keyLen] = 0; // null terminate
	}
	return new;
}

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

static inline s32 
keyCmp(
	u8 *key1,
	u32 key1Len,
	u32 key1Hash,
	u8 *key2,
	u32 key2Len,
	u32 key2Hash)
{
	s32 res;
	
	res = (key1Len-key2Len)|(key1Hash-key2Hash);
	if(res == 0){
		res = HT_CMP(key1,key2);
	}
	
	return res;
}

/*******************************************************************************
 * Section Utilities
*******************************************************************************/

HASHTABLE_STATIC_BUILD
u32
hashTable_maxChain(HashTable *ht)
{
	if(ht==0){
		return 0;
	}
	hashTableNode **table = ht->table;
	u32 chain_count, max=0;
	hashTableNode *cur_node;
	u32 size;
	size = ht->size;
	for(u32 x = 0; x < size; x++)
	{
		cur_node = table[x];
		chain_count = 0;
		while(cur_node){
			// there is atleast one thing here
			chain_count++;
			cur_node=cur_node->next;
		}
		if(chain_count>max){
			max = chain_count;
		}
	}

	return max;
}

HASHTABLE_STATIC_BUILD
u32
hashTable_countEachNode(HashTable *ht)
{
	if(ht==0){
		return 0;
	}
	hashTableNode **table = ht->table;
	hashTableNode *cur_node;
	u32 count=0;
	u32 size;
	size = ht->size;
	for(u32 x = 0; x < size; x++)
	{
		cur_node = table[x];
		while(cur_node){
			// there is atleast one thing here
			count++;
			cur_node = cur_node->next;
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
	hashTableNode *cur_node, *prev_node;
	u32 size;
	if (ht_p==0) {
		return;
	}
	ht = *ht_p;
	*ht_p = 0;
	table = ht->table;
	size = ht->size;
	for(u32 x = 0; x < size; x++)
	{
		cur_node = table[x];
		while(cur_node){
			// there is atleast one thing here
			prev_node = cur_node;
			cur_node = cur_node->next;
			free(prev_node);
		}
	}
	free(table);
	free(ht);
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
	u64 isNegative = (((*string)&0x80)==0);
	
	// skip header
	++string;
	
	while ( (*string >= 1) && (*string <= 128) ) {
		val = (val * 128) + ((*string) - 1);
		++string;
	}
	
	// add back in sign bit
	val = val | (isNegative<<63);
	
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
	s32 x = mainAPIReturnValue;
	switch(x){
		case hashTable_errorNullParam1:
		return (u8*)"hashTable Error: First parameter provided is NULL(0).\n";
		case hashTable_errorNullParam2:
		return (u8*)"hashTable Error: Second parameter provided is NULL(0).\n";
		case hashTable_errorNullParam3:
		return (u8*)"hashTable Error: Third parameter provided is NULL(0).\n";
		case hashTable_errorTableIsEmpty:
		return (u8*)"hashTable Error: tree is empty because it is NULL(0).\n";
		case hashTable_errorMallocFailed:
		return (u8*)"hashTable Error: "
					"Malloc was called and returned NULL(0).\n";
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
