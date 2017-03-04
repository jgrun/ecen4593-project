# Makefile
# Loosely based on https://stackoverflow.com/questions/1484817/how-do-i-make-a-simple-makefile-for-gcc-on-linux
TARGET = sim
CC = gcc
CFLAGS = -Wall -Wextra -pedantic -Wshadow -m64 -std=c11 -Wpointer-arith -masm=intel -save-temps -fverbose-asm -Wstrict-prototypes -Wmissing-prototypes
# -Wall -Wextra -pedantic: stricter warnings
# -Wshadow: warn if a local shadows something else
# -m64: Target x86-64
# -std=c99: ISO/IEC 9899:2011, current C standard
# -save-temps -fverbose-asm -masm=intel: make prettier disassembly
# -Wpointer-arith: warn on silly pointer operations
# --Wstrict-prototypes -Wmissing-prototypes: be strict about function prototypes
LIBS =

.PHONY: test clean
.PRECIOUS: $(TARGET) $(OBJECTS)

# Get all the header files and object files
HEADERS = $(wildcard src/*.h)
OBJECTS = $(patsubst %.c, %.o, $(wildcard src/*.c))

# Build all the object files
%.o: %.c $(HEADERS)
		$(CC) $(CFLAGS) -c $< -o $@

# Build the target
$(TARGET): $(OBJECTS)
		$(CC) $(OBJECTS) -Wall $(LIBS) -o $@

all: $(TARGET)

test: $(OBJECTS)
		$(CC) src/alu.o -Wall $(LIBS) -o test/alu-test test/alu-test.c
		$(CC) src/registers.o -Wall $(LIBS) -o test/registers-test test/registers-test.c
		$(CC) src/decode.o src/registers.o -Wall $(LIBS) -o test/decode-test test/decode-test.c
		$(CC) src/memory.o -Wall $(LIBS) -o test/memory-test test/memory-test.c
		test/alu-test
		test/registers-test
		test/decode-test
		test/memory-test

test-alu: $(OBJECTS)
		$(CC) src/alu.o -Wall $(LIBS) -o test/alu-test test/alu-test.c
		test/alu-test

test-registers: $(OBJECTS)
		$(CC) src/registers.o -Wall $(LIBS) -o test/registers-test test/registers-test.c
		test/registers-test

test-decode: $(OBJECTS)
		$(CC) src/decode.o src/registers.o -Wall $(LIBS) -o test/decode-test test/decode-test.c
		test/decode-test

test-memory: $(OBJECTS)
		$(CC) src/memory.o -Wall $(LIBS) -o test/memory-test test/memory-test.c
		test/memory-test

clean:
		-rm -f *.bc *.i *.s
		-rm -f src/*.o
		-rm -f *.gch src/*.gch
		-rm -f $(TARGET)
		-rm -f test/alu-test
		-rm -f test/registers-test
		-rm -f test/decode-test
		-rm -f test/memory-test
		-rm -f sandbox/test-decode
