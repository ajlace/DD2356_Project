#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <mpi.h>

#include "header.h"

struct Config {
	int rank, rankSize, offset, nrOfChunks;
	int* sendCounters;
	int* recvCounters;
	char* fInName;
	MPI_File inFile;
	hashTable* table;
	hashTable* finalTable;
	vector* sendVectors;
	MPI_Datatype MPI_WORD;
};

struct Config config;

int main(int argc, char* argv[])
{
	int opt;

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &config.rank);
	MPI_Comm_size(MPI_COMM_WORLD, &config.rankSize);

	/* if less than 2 arguments are provided (have to provide -f as argument) */
	if(argc < 2) {
		error(USAGE_ERR, __LINE__);
	}
	
	while ((opt = getopt(argc, argv, "f:")) != -1) {
		switch (opt) {
			case 'f':
				config.fInName = optarg;	// read file name char* into config
				break;
			default:	
				error(USAGE_ERR, __LINE__);
		}
	}
	
	init();			// init all values
	wordCount();	// do word count for input file
	reduce();		// redistribute and reduce
	cleanup();		// free all resources

	MPI_Finalize();	// signal end of MPI
	return 0;		// exit program
}

void init() 
{
	/* init variables */
	int i;
	int chunksForRanks[config.rankSize];
	int offsets[config.rankSize];

	/* open file for all ranks */
	MPI_File_open(MPI_COMM_WORLD, config.fInName, MPI_MODE_RDONLY, MPI_INFO_NULL, &config.inFile);

	/* master rank calculates nr of chunks and offsets for all ranks */
	if(config.rank == MASTER) {
		
		MPI_Offset fileSize;			// variable to hold file size
		MPI_File_get_size(config.inFile, &fileSize);	// read file size in bytes
		
		/* total nr of chunks */
		int chunks = (fileSize / CHUNK) + (fileSize % CHUNK > 0 ? 1 : 0);	
		int chunksPerRank = chunks / config.rankSize;	// chunks for all ranks
		int extraChunks = chunks % config.rankSize;		// chunks for some ranks
		int lastOffset = 0;								// offsets for last rank
		offsets[0] = 0;									// set offset for first rank to 0	

		/* for all ranks, calculate offsets and nr of chunks */
		for(i = 0; i < config.rankSize; i++) {
			chunksForRanks[i] = chunksPerRank;	// chunksPerRank of chunks for all ranks
			
			/* if not the last rank */
			if(i < config.rankSize - 1) {
				offsets[i + 1] = sizeof(char) * (lastOffset + (chunksPerRank * CHUNK));	// set offset for next rank

				/* if there are still extra chunks to be distributed */
				if(extraChunks > 0) {
					chunksForRanks[i]++;			// give extra chunk to this rank
					offsets[i + 1] += sizeof(char) * CHUNK;	// change offset for next rank by chunk size
					extraChunks--;				// remove one extra chunk from counter
				}	
	
				lastOffset = offsets[i + 1];		// sets prev offset to the next ranks offset
			}
		}
	}

	/* Broadcast the nr of chunks and offsets to all ranks */
	MPI_Bcast(chunksForRanks, config.rankSize, MPI_INT, MASTER, MPI_COMM_WORLD);
	MPI_Bcast(offsets, config.rankSize, MPI_INT, MASTER, MPI_COMM_WORLD);

	/* save individual values to the config structs */
	config.nrOfChunks = chunksForRanks[config.rank];
	config.offset = offsets[config.rank];

	/* construct the hash table to be used when reading chunks */
	config.table = construct(config.table);
	config.finalTable = construct(config.finalTable);

	config.sendCounters = (int*) calloc(config.rankSize, sizeof(int));
	config.recvCounters = (int*) malloc(config.rankSize * sizeof(int));
	config.sendVectors = (vector*) malloc(config.rankSize * sizeof(vector));

	for(i = 0; i < config.rankSize; i++) {
		vectorInit(&config.sendVectors[i], VECTOR_INIT_SIZE);
	}
	
	word w;
	MPI_Datatype internal[2] = {MPI_CHAR, MPI_UNSIGNED_LONG};
	int blockLen[2] = {MAX_WORD + 1, 1};
	MPI_Aint disp[2];
	disp[0] = (void*)w.data - (void*)&w;
	disp[1] = (void*)&w.value - (void*)&w;

	MPI_Type_create_struct(2, blockLen, disp, internal, &config.MPI_WORD);
	MPI_Type_commit(&config.MPI_WORD);
	
	// TODO: remove this later
	printf("rank: %d, chunks: %d, offset: %d\n", config.rank, config.nrOfChunks, config.offset);
}

void wordCount() 
{
	int i, j;
	bucket* buckPnt = NULL;
	char* chunkBuff = (char*)malloc(CHUNK * sizeof(char));			// chunk buffer
	char* wordBuff = (char*)malloc((MAX_WORD + 1) * sizeof(char));	// word buffer
	char* charBuff = (char*)malloc(sizeof(char));					// char buffer, set at length 1
	int wPoint = 0;	// pointer to word buffer
	
	// TODO: could possibly be done with open_mp?
	for(i = 0; i < config.nrOfChunks; i++) {

		/* read a chunk from the input file into the chunk buffer. */
		MPI_File_read_at_all(config.inFile, config.offset + (i*CHUNK), chunkBuff, CHUNK, MPI_CHAR, MPI_STATUS_IGNORE);
	
		for(j = 0; j < CHUNK; j++) {		// for each character in the chunk
			charBuff[0] = chunkBuff[j];		// load character from chunk into char buffer
			
			// TODO: Make sure that the last rank does not read past end of file
			if(*charBuff == EOF) {
				printf("REACHED END!!!\n");
				goto end; 			// if eof go to end (double break out of for loops)
			}

			/* if character matches and we can add char */
			if(*charBuff >= 'a' && *charBuff <= 'z' && wPoint < MAX_WORD) {
				wordBuff[wPoint] = *charBuff;	// put character into word buffer
				wPoint++;			// increase word buffer pointer
			}

			/* if the character does not match, or if the word buffer is full */
			if(((*charBuff < 'a' || *charBuff > 'z') && wPoint > 0) || wPoint == MAX_WORD) {	
				wordBuff[wPoint] = '\0';				// mark end of word
				bool newWord = addString(config.table, wordBuff, &buckPnt);	// add to hashTable
				wPoint = 0;						// reset word pointer
				
				if(newWord) {
					int toRank = (int) hash((unsigned char*)wordBuff, config.rankSize);
					config.sendCounters[toRank]++;
				}
			}
		}
	}
	
	/* end label, will go here if end of file is reached */
	end:	
	
	MPI_Barrier(MPI_COMM_WORLD);
	//printf("rank %d stored words: %d\n", config.rank, config.table->size);

	free(chunkBuff);
	free(wordBuff);
	free(charBuff);	
	MPI_File_close(&config.inFile);
}

void reduce() 
{
	int i;
	int* sendRecvBuff = (int*) malloc(config.rankSize * sizeof(int));
	int* pointMem = sendRecvBuff;

	toVectors(config.table, config.sendVectors, config.rankSize);

	for (i = 0; i < config.rankSize; i++) {
		if(i == config.rank) sendRecvBuff = config.sendCounters;

		MPI_Bcast(sendRecvBuff, config.rankSize, MPI_INT, i, MPI_COMM_WORLD);
		config.recvCounters[i] = sendRecvBuff[config.rank];

		if(i == config.rank) sendRecvBuff = pointMem;
	}
	
	free(sendRecvBuff);

	word* recvBuff[config.rankSize];
	int recieves = 0;

	for (i = 0; i < config.rankSize; i++) {
		if(config.recvCounters[i] > 0) {
			recvBuff[i] = malloc(config.recvCounters[i] * sizeof(word));
			recieves++;
		}
	}
	
	MPI_Request ignore;
	MPI_Request requests[recieves];
	recieves = 0;

	for (i = 0; i < config.rankSize; i++) {
		if(i != config.rank) {
			if(config.sendCounters[i] > 0) {
				MPI_Isend(config.sendVectors[i].data, config.sendCounters[i], config.MPI_WORD, i, i, MPI_COMM_WORLD, &ignore);
			}
			
			if(config.recvCounters[i] > 0) {
				MPI_Irecv(recvBuff[i], config.recvCounters[i], config.MPI_WORD, i, config.rank, MPI_COMM_WORLD, &requests[recieves]);
				recieves++;
			}
		}
	}

	MPI_Waitall(recieves, requests, MPI_STATUS_IGNORE);
	int j;
	
	for(i = 0; i < config.rankSize; i++) {
		if(i == config.rank) continue;
		
		for(j = 0; j < config.recvCounters[i]; j++) {
			addWord(config.finalTable, &recvBuff[i][j]);
		}
	}
	
	for(i = 0; i < config.sendCounters[config.rank]; i++) {
		addWord(config.finalTable, &config.sendVectors[config.rank].data[i]);
	}
	
	hashTablePrint(config.finalTable);
}

void error(int errCode, int line) 
{
	if(errCode == USAGE_ERR && config.rank == MASTER) {
		fprintf(stderr, "\nError on line %d\tMandatory flag: -f <file name>\n", line);
	}

	if(errCode == REALLOC_ERR) {
		cleanup();
		if(config.rank == MASTER) {
			fprintf(stderr, "\nError on line %d\tReallocation error\n", line);
		}
	}

	MPI_Finalize();
	exit(errCode);
}

void cleanup() {
	
	hashTableFree(config.table);
	free(config.sendCounters);
	free(config.recvCounters);
	
	int i;
	for(i = 0; i < config.rankSize; i++) {
		vectorFree(&config.sendVectors[i]);
	}
	
	free(config.sendVectors);
}