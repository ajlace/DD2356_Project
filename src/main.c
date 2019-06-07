#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>
#include <mpi.h>

#include "header.h"

struct Config {
	int rank, rankSize;
	unsigned long long fileReads, lastRead;
	int *sendCounters, *recvCounters;
	char *fInName;
	MPI_File inFile;
	hashTable *table, *finalTable;
	vector *sendVectors;
	MPI_Datatype MPI_WORD;
};

struct Config config;

int main(int argc, char *argv[])
{
	int opt, i;
	int repeat = REPEAT;
	double startTime, endTime, runtimeMap, runtimeRed, runtime;
	double avgRuntime = 0.0, 	prevAvgRuntime = 0.0, 	 sdRuntime = 0.0;
	double avgRuntimeMap = 0.0, prevAvgRuntimeMap = 0.0, sdRuntimeMap = 0.0;
	double avgRuntimeRed = 0.0, prevAvgRuntimeRed = 0.0, sdRuntimeRed = 0.0;

	MPI_Init_thread(&argc, &argv, 2, &i);
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
	
	for(i = 0; i < repeat; i++) {
		init();			// init all values
		MPI_Barrier(MPI_COMM_WORLD);
		
		startTime = MPI_Wtime();
		wordCount();	// do word count for input file
		MPI_Barrier(MPI_COMM_WORLD);
		endTime = MPI_Wtime();
				
		runtimeMap = endTime - startTime;
		prevAvgRuntimeMap = avgRuntimeMap;
		avgRuntimeMap = avgRuntimeMap + (runtimeMap - avgRuntimeMap) / (i + 1);
		sdRuntimeMap = sdRuntimeMap + (runtimeMap - avgRuntimeMap) * (runtimeMap - prevAvgRuntimeMap);
		
		MPI_Barrier(MPI_COMM_WORLD);

		startTime = MPI_Wtime();
		reduce();		// redistribute and reduce
		MPI_Barrier(MPI_COMM_WORLD);
		endTime = MPI_Wtime();
		
		runtimeRed = endTime - startTime;
		prevAvgRuntimeRed = avgRuntimeRed;
		avgRuntimeRed = avgRuntimeRed + (runtimeRed - avgRuntimeRed) / (i + 1);
		sdRuntimeRed = sdRuntimeRed + (runtimeRed - avgRuntimeRed) * (runtimeRed - prevAvgRuntimeRed);
				
		cleanup();		// free all resources
		
		runtime = runtimeMap + runtimeRed;
		prevAvgRuntime = avgRuntime;
		avgRuntime = avgRuntime + (runtime - avgRuntime) / (i + 1);
		sdRuntime = sdRuntime + (runtime - avgRuntime) * (runtime - prevAvgRuntime);
		
		MPI_Barrier(MPI_COMM_WORLD);
	}

	sdRuntimeMap = sqrt(sdRuntimeMap / (repeat - 1));
	sdRuntimeRed = sqrt(sdRuntimeRed / (repeat - 1));
	sdRuntime = sqrt(sdRuntime / (repeat - 1));

	//printf("Processes\t= %d\n", config.size);
	if(config.rank == MASTER) {
		printf("\nTotal time\t\t= %fs ± %fs\n", avgRuntime, sdRuntime);
		printf("Map time\t\t= %fs ± %fs\n", avgRuntimeMap, sdRuntimeMap);
		printf("Reduction time\t= %fs ± %fs\n", avgRuntimeRed, sdRuntimeRed);
	}

	MPI_Finalize();	// signal end of MPI
	return 0;		// exit program
}

void init() 
{
	/* init variables */
	int i;

	/* open file for all ranks */
	MPI_File_open(MPI_COMM_WORLD, config.fInName, MPI_MODE_RDONLY, MPI_INFO_NULL, &config.inFile);
	
	MPI_Aint bytesPerChunk = CHUNK * sizeof(char);
	MPI_Aint bytesPerReadAll = config.rankSize * bytesPerChunk;
	MPI_Offset offset = config.rank * bytesPerChunk;
	
	MPI_Datatype contig, filetype;
	MPI_Type_contiguous(CHUNK, MPI_CHAR, &contig);
	MPI_Type_create_resized(contig, 0, bytesPerReadAll, &filetype);
	MPI_Type_commit(&filetype);
	
	MPI_File_set_view(config.inFile, offset, MPI_CHAR, filetype, "native", MPI_INFO_NULL);
	
	MPI_Offset fileSize;							// variable to hold file size
	MPI_File_get_size(config.inFile, &fileSize);	// read file size in bytes
	//printf("rank: %d file size: %lld\n", config.rank, fileSize);
	
	int extraRead = (fileSize % bytesPerReadAll) ? 1 : 0;
	config.fileReads = (unsigned long long) ((fileSize / bytesPerReadAll) + extraRead);
	
	if(extraRead) {
		int fullLastChunks = (fileSize % bytesPerReadAll) / (sizeof(char) * CHUNK);
		if(config.rank < fullLastChunks) {
			config.lastRead = CHUNK;
		} else if(config.rank == fullLastChunks) {
			config.lastRead = (fileSize % bytesPerReadAll) % (sizeof(char) * CHUNK);
		} else {
			config.lastRead = 0;
		}
	}

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
	//printf("rank: %d, fileReads: %lld\n", config.rank, config.fileReads);
}

void wordCount() 
{
	int wPoint;
	unsigned long long i, j;
	bool newWord;
	bucket *buckPnt = NULL;
	char chunkBuff[CHUNK]; 			// chunk buffer
	char wordBuff[MAX_WORD + 1]; 	// word buffer
	char charBuff[1]; 				// char buffer
	
	#pragma omp parallel 
	{
		unsigned long long chunkLength = CHUNK;
		
		#pragma omp for private(i, j, buckPnt, wPoint, chunkBuff, wordBuff, charBuff, newWord)
		for(i = 0; i < config.fileReads; i++) {
			
			// read a chunk from the input file into the chunk buffer.
			#pragma omp critical(fileRead)
			MPI_File_read_all(config.inFile, chunkBuff, CHUNK, MPI_CHAR, MPI_STATUS_IGNORE);
			
			wPoint = 0;
			
			if(i == config.fileReads - 1) chunkLength = config.lastRead;
			
			for(j = 0; j < chunkLength; j++) {	// for each character in the chunk
				charBuff[0] = chunkBuff[j];		// load character from chunk into char buffer
				
				// if character matches and we can add char 
				if(*charBuff >= 'a' && *charBuff <= 'z' && wPoint < MAX_WORD) {
					wordBuff[wPoint] = *charBuff;	// put character into word buffer
					wPoint++;						// increase word buffer pointer
				}
				
				// if the character does not match, or if the word buffer is full
				if(((*charBuff < 'a' || *charBuff > 'z') && wPoint > 0) || wPoint == MAX_WORD) {	
					wordBuff[wPoint] = '\0';	// mark end of word
					
					#pragma omp critical(addToHashTable)
					newWord = addString(config.table, wordBuff, &buckPnt);	// add to hashTable
					wPoint = 0;												// reset word pointer
					
					if(newWord) {
						int toRank = (int) hash((unsigned char*)wordBuff, config.rankSize);
						#pragma omp atomic
						config.sendCounters[toRank]++;
					}
				}
			}
		}

		//printf("rank %d stored words: %d\n", config.rank, config.table->size);
	}
	
	MPI_File_close(&config.inFile);
}

void reduce() 
{
	int i;
	int *sendRecvBuff = (int*) malloc(config.rankSize * sizeof(int));
	int *pointMem = sendRecvBuff;

	toVectors(config.table, config.sendVectors, config.rankSize);

	for (i = 0; i < config.rankSize; i++) {
		if(i == config.rank) sendRecvBuff = config.sendCounters;

		MPI_Bcast(sendRecvBuff, config.rankSize, MPI_INT, i, MPI_COMM_WORLD);
		config.recvCounters[i] = sendRecvBuff[config.rank];

		if(i == config.rank) sendRecvBuff = pointMem;
	}
	
	free(sendRecvBuff);

	word *recvBuff[config.rankSize];
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
	
	config.recvCounters[config.rank] = 0;
	
	for(i = 0; i < config.rankSize; i++) {	
		for(j = 0; j < config.recvCounters[i]; j++) {
			addWord(config.finalTable, &recvBuff[i][j]);
		}
	}
	
	for(i = 0; i < config.sendCounters[config.rank]; i++) {
		addWord(config.finalTable, &config.sendVectors[config.rank].data[i]);
	}
	
	//hashTablePrint(config.finalTable);
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