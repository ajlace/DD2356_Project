CC = cc
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
BIN = ./bin
TARGET = mapRed.out
CORES = 4
TEST = wiki.txt
HOSTS = hosts
EXE = mpirun

LDFLAGS = -fopenmp -std=gnu11 -lm -O3
CFLAGS = -I./include -fopenmp -std=gnu11 -g -Wall -O3

all: dir $(BIN)/$(TARGET)

dir: ${BIN}

${BIN}:
	mkdir -p $(BIN)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(BIN)/$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

run: all
	export OMP_NUM_THREADS=2
	$(EXE) -machinefile $(HOSTS) -np $(CORES) $(BIN)/$(TARGET) -f $(TEST) > out.txt

.PHONY: clean
clean:
	rm -f $(OBJ) $(BIN)/$(TARGET)
