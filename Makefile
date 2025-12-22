# Makefile for Zeal 8-bit C Compiler (Desktop build)

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -Iinclude -g
LDFLAGS =

# Detect architecture
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
    ARCH := darwin
else ifeq ($(UNAME_S),Linux)
    ARCH := linux
else
    ARCH := $(UNAME_S)
endif

# Source files
SRCS = src/cc/main.c src/common/common.c src/common/ast_io.c src/parser/lexer.c src/parser/parser.c src/common/symbol.c src/codegen/codegen.c src/codegen/codegen_strings.c \
       src/target/modern/target_args.c src/target/modern/target_io.c
OBJS = $(SRCS:.c=.o)

# Output binary with architecture suffix
TARGET = bin/cc_$(ARCH)

PARSE_SRCS = src/parser/main_parse.c src/common/common.c src/common/ast_io.c src/parser/lexer.c src/parser/parser.c src/common/symbol.c \
             src/target/modern/target_args.c src/target/modern/target_io.c
PARSE_OBJS = $(PARSE_SRCS:.c=.o)
PARSE_TARGET = bin/cc_parse_$(ARCH)

.PHONY: all clean test

all: $(TARGET) $(PARSE_TARGET)

$(TARGET): $(OBJS)
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o $@ $^

$(PARSE_TARGET): $(PARSE_OBJS)
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
	rm -f $(PARSE_OBJS) $(PARSE_TARGET)
	rm -rf bin/*.o

test: $(TARGET)
	@echo "Testing compiler..."
	@./$(TARGET) tests/simple_return.c tests/simple_return.asm
	@echo "Test output:"
	@cat tests/simple_return.asm

.PHONY: all clean test
