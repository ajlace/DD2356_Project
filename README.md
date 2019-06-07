# DD2356 MapReduce Project

## Git structure:
* master branch: MPI implementation with non-blocking communication
* collective branch: MPI Collective I/O implementation with non-blocking point-to-point communication
* omp branch: MPI Collective I/O OpenMP implementation with non-blocking point-to-point communication

## Beskow set up:
* Change the amount of repetitions in include/header.h
* module swap PrgEnv-xxx PrgEnv-gnu
* compile using Makefile

## Run on beskow:
* MPI: aprun -n X ./mapRed.out -f <fileName>
* Collective: aprun -n X ./mapRedCollective.out -f <fileName>
* OpenMP: aprun -n X ./mapRedOmp.out -f <fileName>
