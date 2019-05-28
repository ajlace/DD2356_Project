
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "header.h"

hashTable* construct(hashTable* table) 
{
	table = (hashTable*) calloc(1, sizeof(hashTable));
	table->maxSize = BUCKETS;
	table->maxKeyLength = MAX_WORD;

	return table;
}

bool addString(hashTable* table, char* key, bucket** destBucket) 
{
	unsigned long hashIndex = hash((unsigned char*)key, table->maxSize);
	bucket* buck = &table->table[hashIndex];
	bucket* last;

	if(buck->value) {							// if first bucket is set
		while(buck) {							// while this bucket is defined
			if(!strcmp(key, buck->key)) { 		// if keys are same
				buck->value++;
				*destBucket = buck;
				return false;
			}

			last = buck;		// save current bucket pointer
			buck = buck->next;	// change pointer to next in linked list
		}
		
		buck = (bucket*) malloc(sizeof(bucket));
		buck->next = NULL;
		buck->value = 0;
		last->next = buck;
		*destBucket = buck;
	}
	
	strcpy(buck->key, key); // copy value from key to bucket
	buck->value++;
	table->size++;
	
	return true;
}

bool addWord(hashTable* table, word* w)
{
	unsigned long hashIndex = hash((unsigned char*)w->data, table->maxSize);
	bucket* buck = &table->table[hashIndex];
	bucket* last;

	if(buck->value) {							// if first bucket is set
		while(buck) {							// while this bucket is defined
			if(!strcmp(w->data, buck->key)) { 	// if keys are same
				buck->value += w->value;
				return false;
			}

			last = buck;		// save current bucket pointer
			buck = buck->next;	// change pointer to next in linked list
		}
		
		buck = (bucket*) malloc(sizeof(bucket));
		buck->next = NULL;
		buck->value = 0;
		last->next = buck;
	}
	
	strcpy(buck->key, w->data); // copy value from key to bucket
	buck->value += w->value;
	table->size++;
	
	return true;
}

void hashTablePrint(hashTable* table) 
{
	int i;
	bucket* buck;
	for(i = 0; i < table->maxSize; i++) {
		buck = &table->table[i];
		while(buck && buck->value) {	// while bucket is not NULL and has a string
			printf("%s %lu\n", buck->key, buck->value);
			buck = buck->next;
		}
	}
}

unsigned long hash(unsigned char* str, unsigned long maxSize) 
{
	unsigned long hash = 5381;
	int c;

	while ((c = *str++)) {
		hash = ((hash << 5) + hash) + c; // hash = hash * 33 + c
	}

	return hash % maxSize;
}

void toVectors(hashTable* table, vector* vect, int vectors) 
{
	int i, index;
	bucket* buck;
	for(i = 0; i < table->maxSize; i++) {
		buck = &table->table[i];
		while(buck && buck->value) {	// while bucket has string
			index = (int) hash((unsigned char*) buck->key, vectors);
			vectorAppend(&vect[index], buck);
			buck = buck->next;
		}
	}
}

void hashTableFree(hashTable* table) 
{
	int index;
	bucket* buck;
	bucket* tmp;
	for(index = 0; index < table->maxSize; index++) {
		buck = table->table[index].next;
		while(buck) {	// while bucket is not NULL
			tmp = buck;
			buck = buck->next;
			free(tmp);
		}
	}

	free(table);
}
