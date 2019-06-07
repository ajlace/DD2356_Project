CC = cc
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
BIN = ./bin
TARGET = mapRed.out
CORES = 4
TEST = wiki.txt
HOSTS = hosts
EXE = mpirun

LDFLAGS = -lm 
CFLAGS = -I./include -g -Wall -O3

all: dir $(BIN)/$(TARGET)

dir: ${BIN}

${BIN}:
	mkdir -p $(BIN)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(BIN)/$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

run: all
	$(EXE) -machinefile $(HOSTS) -np $(CORES) $(BIN)/$(TARGET) -f $(TEST)

.PHONY: clean
clean:
	rm -f $(OBJ) $(BIN)/$(TARGET)
