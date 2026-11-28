# Makefile for Zeal 8-bit C Compiler (Desktop build)

CC = gcc
# Host-only pool size; Zeal uses per-binary defaults.
CFLAGS = -Wall -Wextra -std=c99 -Iinclude -g -DCC_POOL_SIZE=32768
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
# Keep in sync with CMakeLists.txt; use modern target I/O for host builds.
CC_SRCS = src/cc/main.c
CC_OBJS = $(CC_SRCS:.c=.o)

# Output binary with architecture suffix
TARGET = bin/cc_$(ARCH)

PARSE_SRCS = src/parser/main.c src/parser/lexer.c src/parser/parser.c src/common/common.c src/common/type.c src/common/ast_write.c \
             src/target/modern/target_args.c src/target/modern/target_io.c
PARSE_OBJS = $(PARSE_SRCS:.c=.o)
PARSE_TARGET = bin/cc_parse_$(ARCH)

CODEGEN_SRCS = src/codegen/main.c src/codegen/codegen.c src/codegen/codegen_strings.c src/common/common.c src/common/ast_read.c \
               src/common/ast_reader/ast_reader_init.c src/common/ast_reader/ast_reader_load_strings.c src/common/ast_reader/ast_reader_string.c src/common/ast_reader/ast_reader_read_type_info.c \
               src/common/ast_reader/ast_reader_begin_program.c src/common/ast_reader/ast_reader_skip_tag.c src/common/ast_reader/ast_reader_skip_node.c src/common/ast_reader/ast_reader_destroy.c \
               src/target/modern/target_args.c src/target/modern/target_io.c
CODEGEN_OBJS = $(CODEGEN_SRCS:.c=.o)
CODEGEN_TARGET = bin/cc_codegen_$(ARCH)

SEMANTIC_SRCS = src/semantic/main.c src/semantic/semantic.c src/common/common.c src/common/ast_read.c \
                src/common/ast_reader/ast_reader_init.c src/common/ast_reader/ast_reader_load_strings.c src/common/ast_reader/ast_reader_string.c src/common/ast_reader/ast_reader_read_type_info.c \
                src/common/ast_reader/ast_reader_begin_program.c src/common/ast_reader/ast_reader_skip_tag.c src/common/ast_reader/ast_reader_skip_node.c src/common/ast_reader/ast_reader_destroy.c \
                src/target/modern/target_args.c src/target/modern/target_io.c
SEMANTIC_OBJS = $(SEMANTIC_SRCS:.c=.o)
SEMANTIC_TARGET = bin/cc_semantic_$(ARCH)

AST_DUMP_SRCS = src/tools/ast_dump.c src/common/common.c src/common/type.c src/common/ast_read.c \
                src/common/ast_reader/ast_reader_init.c src/common/ast_reader/ast_reader_load_strings.c src/common/ast_reader/ast_reader_string.c \
                src/common/ast_reader/ast_reader_read_type_info.c src/common/ast_reader/ast_reader_begin_program.c src/common/ast_reader/ast_reader_destroy.c src/common/ast_reader/ast_reader_skip_tag.c src/common/ast_reader/ast_reader_skip_node.c \
                src/target/modern/target_io.c
AST_DUMP_OBJS = $(AST_DUMP_SRCS:.c=.o)
AST_DUMP_TARGET = bin/ast_dump_$(ARCH)

ALL_OBJS = $(sort $(CC_OBJS) $(PARSE_OBJS) $(CODEGEN_OBJS) $(SEMANTIC_OBJS) $(AST_DUMP_OBJS))
DEPS = $(ALL_OBJS:.o=.d)

.DEFAULT_GOAL := all
-include $(DEPS)

.PHONY: all clean test

all: $(TARGET) $(PARSE_TARGET) $(CODEGEN_TARGET) $(SEMANTIC_TARGET) $(AST_DUMP_TARGET)

# Also replaces objects made before dependency tracking was introduced.
$(ALL_OBJS): Makefile

$(TARGET): $(CC_OBJS)
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o $@ $^

$(PARSE_TARGET): $(PARSE_OBJS)
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o $@ $^

$(CODEGEN_TARGET): $(CODEGEN_OBJS)
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o $@ $^

$(SEMANTIC_TARGET): $(SEMANTIC_OBJS)
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o $@ $^

$(AST_DUMP_TARGET): $(AST_DUMP_OBJS)
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

clean:
	rm -f $(CC_OBJS) $(TARGET)
	rm -f $(PARSE_OBJS) $(PARSE_TARGET)
	rm -f $(CODEGEN_OBJS) $(CODEGEN_TARGET)
	rm -f $(SEMANTIC_OBJS) $(SEMANTIC_TARGET)
	rm -f $(AST_DUMP_OBJS) $(AST_DUMP_TARGET)
	rm -rf bin/*.o
	rm -f $(DEPS)

test: all
	python3 -m unittest discover -s tests -p 'test_*.py'
	./test.sh

.PHONY: all clean test

export ZDE_STRICT_EXIT := 1
CASE ?= simple_return
STAGE ?= parse
.PHONY: dist accept debug debug-smoke

dist:
	zde --no-tty cmake

accept: dist ensure-media
	python3 test.py --headless-only --no-build

debug: dist ensure-media
	python3 test.py --debug-case "$(CASE)" --stage "$(STAGE)" --no-build

debug-smoke: dist ensure-media
	python3 test.py --debug-smoke --no-build

.PHONY: test-media
test-media:
	zde --no-tty deps stage romdisk coreutils
	zde --no-tty deps stage romdisk zshell
	zde --no-tty exec python3 tests/build_media.py

.PHONY: ensure-media
ensure-media:
	@python3 -c 'from stage import media; import sys; sys.exit(not (media("headless-tf.img","ZEAL_CC_ROM").is_file() and media("zealasm","ZEALASM").is_file()))' || $(MAKE) test-media
