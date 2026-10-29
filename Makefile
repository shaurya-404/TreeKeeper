# TreeKeeper Makefile
#   make        - build a.out
#   make debug  - build with debug info (for gdb / valgrind)
#   make test   - build and run the test program
#   make run    - build and run
#   make clean  - delete object files and executables

CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -pthread
LIBS = -pthread

SRC = $(wildcard Source/*.c)
OBJ = $(patsubst Source/%.c, object/%.o, $(SRC))
LIB_OBJ = $(filter-out object/main.o, $(OBJ))

all: a.out

a.out: $(OBJ)
	$(CC) $(OBJ) -o a.out $(LIBS)

object/%.o: Source/%.c include/*.h
	@mkdir -p object
	$(CC) $(CFLAGS) -c $< -o $@

debug: CFLAGS += -g -O0
debug: clean all

test: $(LIB_OBJ) Tests/test.c
	$(CC) $(CFLAGS) Tests/test.c $(LIB_OBJ) -o object/test $(LIBS)
	./object/test

run: a.out
	./a.out

clean:
	rm -rf object a.out

.PHONY: all debug test run clean
