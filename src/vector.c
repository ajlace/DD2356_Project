#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "header.h"

vector* vectorInit(vector* vect, size_t initCapacity)
{
        vect->data = (word*) malloc(initCapacity * sizeof(word));
        vect->capacity = initCapacity;
        vect->size = 0;

        return vect;
}

void vectorResize(vector* vect, size_t newSize)
{
        word* newMem = realloc(vect->data, newSize * sizeof(word));
        if(!newMem) error(REALLOC_ERR, __LINE__);
        vect->data = newMem;
		vect->capacity = newSize;
}

void vectorAppend(vector* vect, bucket* buck)
{
        if(vect->size == vect->capacity - 1) {
                vectorResize(vect, vect->capacity * 2);
        }

        word* w = &vect->data[vect->size];
        strcpy(w->data, buck->key);
        w->value = buck->value;
        vect->size++;
}

void vectorFree(vector* vect)
{
        free(vect->data);
}
