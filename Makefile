# paths
MODULES = ./modules
INCLUDE = ./include
SRC = ./src

# compiler
CC = gcc

# Compile options. Το -I<dir> λέει στον compiler να αναζητήσει εκεί include files
CFLAGS = -Wall -Werror -O3 -march=native -I$(INCLUDE)
LDFLAGS =

# Αρχεία .o
OBJS = $(SRC)/ipli-fast.o $(SRC)/parser.o $(SRC)/interpreter.o $(MODULES)/UsingDynamicArray/ADTVector.o $(MODULES)/UsingAVL/ADTSet.o $(MODULES)/UsingADTSet/ADTMap.o

# Το εκτελέσιμο πρόγραμμα
EXEC = ipli-fast

# Παράμετροι για δοκιμαστική εκτέλεση
ARGS = misc/programs/nqueens.ipl

$(EXEC): $(OBJS)
	$(CC) $(OBJS) -o $(EXEC) $(LDFLAGS)

clean:
	rm -f $(OBJS) $(EXEC)

run: $(EXEC)
	./$(EXEC) $(ARGS)