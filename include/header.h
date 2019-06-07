#ifndef __TYPEDEFS_H__
#define __TYPEDEFS_H__

#include <stdbool.h>

typedef struct word word;
typedef struct bucket bucket;
typedef struct hashTable hashTable;
typedef struct vector vector;

//==================================================
// MAIN
//==================================================

#define MAX_WORD 15
#define MASTER 0
#define CHUNK 67108864ULL
#define USAGE_ERR 1
#define REALLOC_ERR 2
#define REPEAT 10

typedef struct word {
        char data[MAX_WORD + 1];
        unsigned long value;
} word;

void init();
void wordCount();
void reduce();
void cleanup();
void error(int, int);

//==================================================
// HASH TABLE
//==================================================

#define BUCKETS 1000

typedef struct bucket
{
	bucket* next;
	char key[MAX_WORD + 1];
	unsigned long value;
} bucket;

typedef struct hashTable 
{
	bucket table[BUCKETS];
	int size;
	unsigned long maxSize;
	int maxKeyLength;
} hashTable;

hashTable* construct(hashTable*); 
bool addString(hashTable*, char*, bucket**);
bool addWord(hashTable*, word*);
void hashTablePrint(hashTable*);
unsigned long hash(unsigned char*, unsigned long);
void toVectors(hashTable*, vector*, int);
void hashTableFree(hashTable*);

//==================================================
// VECTOR
//==================================================

#define VECTOR_INIT_SIZE 100

typedef struct vector {
        word* data;
        size_t capacity;
        size_t size;
} vector;

vector* vectorInit(vector*, size_t);
void vectorResize(vector*, size_t);
void vectorAppend(vector*, bucket*);
void vectorFree(vector*);

#endif
