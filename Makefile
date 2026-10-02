CC     ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Wpedantic -Iinclude
BUILD  ?= build

CORE_SRC  := src/chip8.c src/ops.c src/disasm.c
HOST_SRC  := src/frontend.c src/terminal.c src/rom.c src/state.c src/main.c
TEST_SRC  := $(CORE_SRC) $(wildcard src/tests/test_*.c)
HEADERS   := include/chip8.h include/disasm.h include/frontend.h \
             include/rom.h include/state.h src/tests/test.h

.PHONY: all test run clean snake play-snake ping play-ping life play-life play

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

SNAKE_ROM := roms/snake.ch8

snake: $(SNAKE_ROM)

$(SNAKE_ROM): roms/snake.asm roms/asm.py
	python3 roms/asm.py

play-snake: $(BUILD)/chip8 $(SNAKE_ROM)
	$(BUILD)/chip8 $(SNAKE_ROM) -s 120 -q modern


PING_ROM := roms/ping.ch8

ping: $(PING_ROM)

$(PING_ROM): roms/ping.asm roms/asm.py
	python3 roms/asm.py roms/ping.asm roms/ping.ch8

play-ping: $(BUILD)/chip8 $(PING_ROM)
	$(BUILD)/chip8 $(PING_ROM) -s 120 -q modern


LIFE_ROM := roms/life.ch8

life: $(LIFE_ROM)

$(LIFE_ROM): roms/life.asm roms/asm.py
	python3 roms/asm.py roms/life.asm roms/life.ch8

play-life: $(BUILD)/chip8 $(LIFE_ROM)
	$(BUILD)/chip8 $(LIFE_ROM) -s 16000 -q modern

play: $(BUILD)/chip8 $(PING_ROM) $(SNAKE_ROM) $(LIFE_ROM)
	$(BUILD)/chip8 --menu

clean:
	rm -rf $(BUILD)
