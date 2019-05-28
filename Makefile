CC = mpicc
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
BIN = ./bin
TARGET = mapRed.out
CORES = 8
TEST = ajla.txt
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
	$(CC) $(LDFLAGS) -o $@ $^

run: all
	$(EXE) -machinefile $(HOSTS) -np $(CORES) $(BIN)/$(TARGET) -f $(TEST)

.PHONY: clean
clean:
	rm -f $(OBJ) $(BIN)/$(TARGET)
