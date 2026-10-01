CC     ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Wpedantic -Iinclude
BUILD  ?= build

CORE_SRC  := src/chip8.c src/ops.c
HOST_SRC  := src/terminal.c src/rom.c src/main.c
TEST_SRC  := $(CORE_SRC) $(wildcard src/tests/test_*.c)
HEADERS   := include/chip8.h include/terminal.h include/rom.h src/tests/test.h

.PHONY: all test run clean

all: $(BUILD)/chip8

$(BUILD)/chip8: $(CORE_SRC) $(HOST_SRC) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(CORE_SRC) $(HOST_SRC) -o $@

$(BUILD)/test_chip8: $(TEST_SRC) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(TEST_SRC) -o $@

$(BUILD):
	mkdir -p $(BUILD)

test: $(BUILD)/test_chip8
	$(BUILD)/test_chip8

run: $(BUILD)/chip8
	$(BUILD)/chip8

rom: $(BUILD)/chip8
	$(BUILD)/chip8 $(ROM)

clean:
	rm -rf $(BUILD)
