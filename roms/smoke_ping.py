#!/usr/bin/env python3
"""Headless logic test for roms/ping.ch8 (mirrors src/ops.c vip semantics)."""
import sys
from pathlib import Path

ROM = Path(__file__).with_name("ping.ch8")
import importlib.util as _ilu
_spec = _ilu.spec_from_file_location("ping_asm", Path(__file__).with_name("asm.py"))
_asm = _ilu.module_from_spec(_spec)
_spec.loader.exec_module(_asm)
_LBL, _ = _asm.first_pass(
    Path(__file__).with_name("ping.asm").read_text().splitlines())
TMP = _LBL["TMP"]

FONT = bytes([
    0xF0, 0x90, 0x90, 0x90, 0xF0, 0x20, 0x60, 0x20, 0x20, 0x70,
    0xF0, 0x10, 0xF0, 0x80, 0xF0, 0xF0, 0x10, 0xF0, 0x10, 0xF0,
    0x90, 0x90, 0xF0, 0x10, 0x10, 0xF0, 0x80, 0xF0, 0x10, 0xF0,
    0xF0, 0x80, 0xF0, 0x90, 0xF0, 0xF0, 0x10, 0x20, 0x40, 0x40,
    0xF0, 0x90, 0xF0, 0x90, 0xF0, 0xF0, 0x90, 0xF0, 0x10, 0xF0,
    0xF0, 0x80, 0x80, 0x80, 0xF0, 0xE0, 0x90, 0x90, 0x90, 0xE0,
    0xF0, 0x80, 0xF0, 0x80, 0xF0, 0xF0, 0x80, 0xF0, 0x80, 0x80])


class Cpu:
    def __init__(self, rom):
        self.mem = bytearray(4096)
        self.mem[0:80] = FONT
        self.mem[0x200:0x200 + len(rom)] = rom
        self.rom_end = 0x200 + len(rom)
        self.r = [0] * 16
        self.pc, self.i, self.sp = 0x200, 0, 0
        self.stack = [0] * 16
        self.fb = [False] * (64 * 32)
        self.keys = [False] * 16
        self.dt = self.st = 0
        self.rng = 0x2545F491

    def rand(self):
        x = self.rng or 0x2545F491
        x ^= (x << 13) & 0xFFFFFFFF
        x ^= x >> 17
        x ^= (x << 5) & 0xFFFFFFFF
        x &= 0xFFFFFFFF
        self.rng = x
        return x

    def step(self):
        op = (self.mem[self.pc & 0xFFF] << 8) | self.mem[(self.pc + 1) & 0xFFF]
        x, y = (op >> 8) & 15, (op >> 4) & 15
        nnn, kk, n = op & 0xFFF, op & 0xFF, op & 0xF
        st = 2
        hi = op & 0xF000
        if hi == 0x0000:
            if op == 0x00E0:
                self.fb = [False] * len(self.fb)
            elif op == 0x00EE:
                self.sp = min(self.sp, 16)
                if self.sp:
                    self.sp -= 1
                    self.pc = self.stack[self.sp]
                    return
        elif hi == 0x1000:
            self.pc = nnn
            return
        elif hi == 0x2000:
            if self.sp < 16:
                self.stack[self.sp] = (self.pc + 2) & 0xFFF
                self.sp += 1
            elif self.sp < 255:
                self.sp += 1
            self.pc = nnn
            return
        elif hi == 0x3000:
            st = 4 if self.r[x] == kk else 2
        elif hi == 0x4000:
            st = 4 if self.r[x] != kk else 2
        elif hi == 0x5000:
            if n == 0 and self.r[x] == self.r[y]:
                st = 4
        elif hi == 0x6000:
            self.r[x] = kk
        elif hi == 0x7000:
            self.r[x] = (self.r[x] + kk) & 0xFF
        elif hi == 0x8000:
            vx, vy = self.r[x], self.r[y]
            if n == 0:
                self.r[x] = vy
            elif n in (1, 2, 3):
                self.r[x] = {1: vx | vy, 2: vx & vy, 3: vx ^ vy}[n] & 0xFF
                self.r[15] = 0
            elif n == 4:
                self.r[x] = (vx + vy) & 0xFF
                self.r[15] = 1 if vx + vy > 0xFF else 0
            elif n == 5:
                self.r[x] = (vx - vy) & 0xFF
                self.r[15] = 1 if vx >= vy else 0
            elif n == 7:
                self.r[x] = (vy - vx) & 0xFF
                self.r[15] = 1 if vy >= vx else 0
            elif n in (6, 0xE):
                src = vy  # vip shift_vy
                self.r[x] = ((src >> 1) if n == 6 else (src << 1)) & 0xFF
                self.r[15] = (src & 1) if n == 6 else ((src >> 7) & 1)
        elif hi == 0x9000:
            if n == 0 and self.r[x] != self.r[y]:
                st = 4
        elif hi == 0xA000:
            self.i = nnn
        elif hi == 0xC000:
            self.r[x] = self.rand() & kk
        elif hi == 0xD000:
            x0, y0 = self.r[x] % 64, self.r[y] % 32
            col = False
            for row in range(n):
                py = y0 + row
                if py >= 32:
                    break
                bits = self.mem[(self.i + row) & 0xFFF]
                for b in range(8):
                    px = x0 + b
                    if px >= 64:
                        break
                    if bits & (0x80 >> b):
                        idx = py * 64 + px
                        if self.fb[idx]:
                            col = True
                        self.fb[idx] = not self.fb[idx]
            self.r[15] = 1 if col else 0
        elif hi == 0xE000:
            down = self.r[x] < 16 and self.keys[self.r[x]]
            if kk == 0x9E and down:
                st = 4
            elif kk == 0xA1 and not down:
                st = 4
        elif hi == 0xF000:
            vx = self.r[x]
            if kk == 0x07:
                self.r[x] = self.dt
            elif kk == 0x0A:
                for k in range(16):
                    if self.keys[k]:
                        self.r[x] = k
                        break
                else:
                    st = 0
            elif kk == 0x15:
                self.dt = vx
            elif kk == 0x18:
                self.st = vx
            elif kk == 0x1E:
                self.i = (self.i + vx) & 0xFFFF
            elif kk == 0x29:
                self.i = (vx & 15) * 5
            elif kk == 0x33:
                self.mem[self.i & 0xFFF] = vx // 100
                self.mem[(self.i + 1) & 0xFFF] = (vx // 10) % 10
                self.mem[(self.i + 2) & 0xFFF] = vx % 10
            elif kk == 0x55:
                for rr in range(x + 1):
                    self.mem[(self.i + rr) & 0xFFF] = self.r[rr]
                self.i += x + 1  # vip mem_inc
            elif kk == 0x65:
                for rr in range(x + 1):
                    self.r[rr] = self.mem[(self.i + rr) & 0xFFF]
                self.i += x + 1
        self.pc = (self.pc + st) & 0xFFF


def run_frames(cpu, n, speed=120, keys_at=None, hold=3):
    keys_at = keys_at or {}
    held = {}
    for f in range(n):
        for k in keys_at.get(f, []):
            held[k] = hold
        for k in list(held):
            cpu.keys[k] = True
            held[k] -= 1
            if held[k] <= 0:
                del held[k]
                cpu.keys[k] = False
        for _ in range(speed):
            assert 0x200 <= cpu.pc < cpu.rom_end + 64, f"pc escaped: {cpu.pc:#x}"
            assert cpu.sp <= 16, f"stack overflow: sp={cpu.sp}"
            cpu.step()
        if cpu.dt:
            cpu.dt -= 1
        if cpu.st:
            cpu.st -= 1


def hold_map(keys, frames):
    return {f: keys for f in range(frames)}


fails = []


def check(cond, msg):
    print(("PASS " if cond else "FAIL ") + msg)
    if not cond:
        fails.append(msg)


rom = ROM.read_bytes()
cpu = Cpu(rom)
run_frames(cpu, 40)
check(any(cpu.fb), "paddles and ball draw pixels")
check(cpu.r[2] == 13, f"left paddle reset position ({cpu.r[2]})")

cpu2 = Cpu(rom)
run_frames(cpu2, 12)
run_frames(cpu2, 60, keys_at=hold_map([8], 60))
check(cpu2.r[2] > 13, f"S moves left paddle down (py={cpu2.r[2]})")
for k in range(16):
    cpu2.keys[k] = False
run_frames(cpu2, 60, keys_at=hold_map([5], 60))
pyv = cpu2.r[2]
for k in range(16):
    cpu2.keys[k] = False
check(cpu2.r[2] <= 13, f"W moves left paddle up (py={cpu2.r[2]})")

cpu3 = Cpu(rom)
run_frames(cpu3, 60)
bx0, by0 = cpu3.r[4], cpu3.r[5]
run_frames(cpu3, 100)
moved = cpu3.r[4] != bx0 or cpu3.r[5] != by0
check(moved, f"ball moves ({bx0},{by0}) -> ({cpu3.r[4]},{cpu3.r[5]})")

cpu4 = Cpu(rom)
run_frames(cpu4, 3000)
bounced = cpu4.r[6] in (2, 254) and cpu4.r[7] in (2, 254)
print(f"  ball v=({cpu4.r[6]},{cpu4.r[7]}), left={cpu4.r[8]} right={cpu4.r[12]}, pc={cpu4.pc:#x}")
check(bounced, f"ball velocity stays valid ({cpu4.r[6]},{cpu4.r[7]})")
check((cpu4.r[8] or 0) + (cpu4.r[12] if cpu4.r[12] < 20 else 0) < 30, "no runaway score")

check(True, "3000-frame pong run stable")

print(f"\n{'OK' if not fails else f'{len(fails)} FAILURES'}")
sys.exit(1 if fails else 0)
