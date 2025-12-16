# Makefile for Zeal 8-bit C Compiler (Desktop build)

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -Iinclude -g
LDFLAGS =

# Source files
SRCS = src/main.c src/common.c src/lexer.c src/parser.c src/symbol.c src/codegen.c
OBJS = $(SRCS:.c=.o)

# Output binary
TARGET = bin/cc

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
	rm -rf bin/*.o

test: $(TARGET)
	@echo "Testing compiler..."
	@./$(TARGET) tests/test1.c tests/test1.asm
	@echo "Test output:"
	@cat tests/test1.asm

.PHONY: all clean test
