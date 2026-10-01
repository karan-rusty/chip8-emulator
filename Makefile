CC     ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Wpedantic -Iinclude
BUILD  ?= build

EMULATOR_SRC := src/chip8.c src/main.c
TEST_SRC     := src/chip8.c src/tests/test_chip8.c
HEADERS      := include/chip8.h

.PHONY: all test run clean

all: $(BUILD)/chip8

$(BUILD)/chip8: $(EMULATOR_SRC) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(EMULATOR_SRC) -o $@

$(BUILD)/test_chip8: $(TEST_SRC) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(TEST_SRC) -o $@

$(BUILD):
	mkdir -p $(BUILD)

test: $(BUILD)/test_chip8
	$(BUILD)/test_chip8

# Run the built-in test ROM and dump the framebuffer as text.
run: $(BUILD)/chip8
	$(BUILD)/chip8

# Run a ROM: make rom ROM=path/to/game.ch8
rom: $(BUILD)/chip8
	$(BUILD)/chip8 $(ROM)

clean:
	rm -rf $(BUILD)
