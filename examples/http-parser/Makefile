# Define compiler and flags
CC = gcc
CFLAGS += -O3 -g
LDFLAGS += -L$(RTE_LIBRTE_PMD) -lrte_pmd

# Define the executable
EXE = http-parser

# Source file
SRC = main.c

# Build target
all: $(EXE)

$(EXE): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

clean:
	rm -f $(EXE)
