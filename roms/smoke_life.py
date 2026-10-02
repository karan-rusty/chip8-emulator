#!/usr/bin/env python3
"""Headless logic test for roms/life.ch8 (mirrors src/ops.c semantics)."""
import sys
from pathlib import Path

ROM = Path(__file__).with_name("life.ch8")
import importlib.util as _ilu
_spec = _ilu.spec_from_file_location("life_asm", Path(__file__).with_name("asm.py"))
_asm = _ilu.module_from_spec(_spec)
_spec.loader.exec_module(_asm)
LBL, _ = _asm.first_pass(Path(__file__).with_name("life.asm").read_text().splitlines())

FONT = bytes([
    0xF0, 0x90, 0x90, 0x90, 0xF0, 0x20, 0x60, 0x20, 0x20, 0x70,
    0xF0, 0x10, 0xF0, 0x80, 0xF0, 0xF0, 0x10, 0xF0, 0x10, 0xF0,
    0x90, 0x90, 0xF0, 0x10, 0x10, 0xF0, 0x80, 0xF0, 0x10, 0xF0,
    0xF0, 0x80, 0xF0, 0x90, 0xF0, 0xF0, 0x10, 0x20, 0x40, 0x40,
    0xF0, 0x90, 0xF0, 0x90, 0xF0, 0xF0, 0x90, 0xF0, 0x10, 0xF0,
    0xF0, 0x80, 0x80, 0x80, 0xF0, 0xE0, 0x90, 0x90, 0x90, 0xE0,
    0xF0, 0x80, 0xF0, 0x80, 0xF0, 0xF0, 0x80, 0xF0, 0x80, 0x80])

# Generation speed used by every test below. Logic is frame-agnostic; the
# only requirement is that a pressed key lasts longer than one generation,
# and every key test here happens while paused (no generation is in flight,
# so KEYS is polled every spin of the delay loop).
SPEED = 1000


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
        # instrumentation: exact instruction counts and generation flips
        self.steps = 0
        self.flips = []          # steps at which V8 (the buffer anchor) changed
        self._v8 = 0

    def rand(self):
        x = self.rng or 0x2545F491
        x ^= (x << 13) & 0xFFFFFFFF
        x ^= x >> 17
        x ^= (x << 5) & 0xFFFFFFFF
        x &= 0xFFFFFFFF
        self.rng = x
        return x

    def step(self):
        if self.r[8] != self._v8:          # DOGEN swapped the buffers
            self._v8 = self.r[8]
            self.flips.append(self.steps)
        self.steps += 1
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

    # own model, not in smoke_ping: count executed instructions per frame
    def run_frame(self, speed):
        for _ in range(speed):
            assert 0x200 <= self.pc < self.rom_end + 64, f"pc escaped: {self.pc:#x}"
            assert self.sp <= 16, f"stack overflow: sp={self.sp}"
            self.step()
        if self.dt:
            self.dt -= 1
        if self.st:
            self.st -= 1


def run_frames(cpu, n, speed=SPEED, keys_at=None, hold=3):
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
        cpu.run_frame(speed)


def hold_map(keys, frames):
    return {f: keys for f in range(frames)}


def release(cpu):
    for k in range(16):
        cpu.keys[k] = False


fails = []


def check(cond, msg):
    print(("PASS " if cond else "FAIL ") + msg)
    if not cond:
        fails.append(msg)


# ---------------------------------------------------------------- reference
def life_step(src):
    """One Conway step: interior cells follow the rules, border is dead."""
    out = [[0] * 16 for _ in range(16)]
    for y in range(1, 15):
        for x in range(1, 15):
            n = 0
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    if dx or dy:
                        n += src[y + dy][x + dx]
            out[y][x] = 1 if (n == 3 or (src[y][x] and n == 2)) else 0
    return out


def grid_at(cpu, base):
    return [[cpu.mem[base + y * 16 + x] for x in range(16)] for y in range(16)]


def plant(cpu, base, g):
    for y in range(16):
        for x in range(16):
            cpu.mem[base + y * 16 + x] = g[y][x] & 1


def fill(cpu, base, byte):
    for o in range(256):
        cpu.mem[base + o] = byte


def borders_dead(g):
    return (all(g[y][x] == 0 for x in range(16) for y in (0, 15)) and
            all(g[y][x] == 0 for y in range(16) for x in (0, 15)))


def fb_matches(cpu, g):
    for y in range(16):
        for x in range(16):
            px, py = 4 * x, 2 * y
            lit = all(cpu.fb[(py + dy) * 64 + px + dx]
                      for dy in range(2) for dx in range(4))
            if lit != bool(g[y][x]):
                return False, f"cell ({x},{y}) is {'lit' if lit else 'dark'}"
    return True, "ok"


def tap(cpu, key, frames=4, speed=SPEED):
    run_frames(cpu, frames, speed=speed, keys_at=hold_map([key], frames))
    release(cpu)


def step_once(cpu, speed=SPEED):
    """Tap the step key; return True if exactly one generation followed."""
    before = len(cpu.flips)
    tap(cpu, 9, speed=speed)
    for _ in range(400):
        if len(cpu.flips) > before:
            break
        run_frames(cpu, 1, speed=speed)
    run_frames(cpu, 40, speed=speed)   # nothing more may happen while paused
    return len(cpu.flips) == before + 1


dirs_seen = set()


def plant_and_step(cpu, pattern, label):
    """Park a pattern in the current buffer, step once, check the result."""
    cur = LBL["GRID_A"] + cpu.r[8]
    nxt = LBL["GRID_A"] + cpu.r[12]
    check(cpu.r[12] == 255 - cpu.r[8],
          f"{label}: buffer anchors are complementary (V8={cpu.r[8]}, VC={cpu.r[12]})")
    dirs_seen.add(cpu.r[8])
    fill(cpu, nxt, 0xAA)               # destination first ...
    plant(cpu, cur, pattern)           # ... source last (they share one byte)
    check(cpu.mem[LBL["GRID_A"] + 255] == 0, f"{label}: shared border byte is dead")
    check(step_once(cpu), f"{label}: step key runs exactly one generation")

    raw = bytes(cpu.mem[nxt:nxt + 256])
    check(all(b <= 1 for b in raw), f"{label}: generation rewrites every cell")
    got = grid_at(cpu, nxt)
    check(got == life_step(pattern), f"{label}: next state matches reference Life")
    check(borders_dead(got), f"{label}: border cells stay dead")
    lit, where = fb_matches(cpu, got)
    check(lit, f"{label}: framebuffer draws the result ({where})")
    return got


# ------------------------------------------------------------------- tests
rom = ROM.read_bytes()
cpu = Cpu(rom)

run_frames(cpu, 4)
check(any(cpu.fb), "seeded pattern draws pixels")

# A 20-frame hold cannot fit inside one generation, so the pause key is
# always observed; the latch must then keep it to a single toggle.
tap(cpu, 5, frames=20)
run_frames(cpu, 16)                    # release + let any in-flight work finish
check(cpu.mem[LBL["PAUSE"]] == 1, "W key pauses the simulation")
v8 = cpu.r[8]
run_frames(cpu, 40)
check(cpu.r[8] == v8, "paused: no generation runs")

# Mixed pattern: blinker, block, glider and live border cells (border cells
# must count as neighbours yet never survive into the next generation).
mixed = [[0] * 16 for _ in range(16)]
for x in (4, 5, 6):
    mixed[5][x] = 1                    # blinker
mixed[9][9] = mixed[9][10] = mixed[10][9] = mixed[10][10] = 1   # block
mixed[2][1] = mixed[3][2] = mixed[4][1] = mixed[4][2] = mixed[4][3] = 1  # glider
mixed[0][3] = mixed[6][0] = mixed[7][15] = mixed[15][10] = 1    # border cells
plant_and_step(cpu, mixed, "mixed")

# Hand-checked still life: a block must not move.
block = [[0] * 16 for _ in range(16)]
block[5][5] = block[5][6] = block[6][5] = block[6][6] = 1
got = plant_and_step(cpu, block, "block")
check(got == block, "block: still life is unchanged")

# Hand-checked oscillator: a horizontal blinker turns vertical.
blink = [[0] * 16 for _ in range(16)]
blink[8][4] = blink[8][5] = blink[8][6] = 1
want = [[0] * 16 for _ in range(16)]
want[7][5] = want[8][5] = want[9][5] = 1
got = plant_and_step(cpu, blink, "blinker")
check(got == want, "blinker: horizontal turns vertical")

check(dirs_seen == {0, 255},
      f"both buffer directions exercised (V8 was {sorted(dirs_seen)})")

# A step key held for 10 frames must still run exactly one generation.
before = len(cpu.flips)
tap(cpu, 9, frames=10)
run_frames(cpu, 80)
check(len(cpu.flips) == before + 1, "held step key runs exactly one generation")

# Reseed while paused: random interior, dead border, redrawn immediately.
cur = LBL["GRID_A"] + cpu.r[8]
before_grid = bytes(cpu.mem[cur:cur + 256])
tap(cpu, 7, frames=6)
run_frames(cpu, 12)
cur = LBL["GRID_A"] + cpu.r[8]
after_grid = bytes(cpu.mem[cur:cur + 256])
live = sum(after_grid[y * 16 + x] for y in range(1, 15) for x in range(1, 15))
check(after_grid != before_grid, "A key reseeds the current buffer")
check(live >= 20, f"reseed plants a random pattern ({live} live cells)")
check(borders_dead(grid_at(cpu, cur)), "reseed leaves the border dead")
lit, where = fb_matches(cpu, grid_at(cpu, cur))
check(lit, f"reseed redraws the new pattern ({where})")

# Pace toggle: 8 delay ticks <-> flat out.
check(cpu.mem[LBL["PACING"]] == 8, "pace starts at 8 delay ticks")
tap(cpu, 8, frames=6)
run_frames(cpu, 12)
check(cpu.mem[LBL["PACING"]] == 0, "S key switches the pace to flat out")
tap(cpu, 8, frames=6)
run_frames(cpu, 12)
check(cpu.mem[LBL["PACING"]] == 8, "S key switches the pace back")

# Resume and let it run: no runaway, no bad jumps, generations keep coming.
tap(cpu, 5, frames=6)
run_frames(cpu, 12)
check(cpu.mem[LBL["PAUSE"]] == 0, "W key resumes the simulation")
flips0 = len(cpu.flips)
run_frames(cpu, 400)                   # run_frames asserts pc and stack
check(len(cpu.flips) - flips0 >= 10,
      f"400-frame run stays healthy ({len(cpu.flips) - flips0} generations)")
check(cpu.mem[LBL["GRID_A"] + 255] == 0, "shared border byte stays dead")

# Size of one generation decides the play speed: the whole update + redraw
# must fit inside a single -s frame or the screen would show half a step.
cpu5 = Cpu(rom)
run_frames(cpu5, 30)
cpu5.mem[LBL["PACING"]] = 0            # flat out: every iteration is one step
cpu5.flips.clear()
run_frames(cpu5, 4, speed=50000)
deltas = [b - a for a, b in zip(cpu5.flips, cpu5.flips[1:])]
gen = sorted(deltas)[len(deltas) // 2] if deltas else 0
print(f"  one generation = {gen} instructions "
      f"(min {min(deltas)} max {max(deltas)})" if deltas else "  no data")
check(0 < gen < 16000, f"a generation fits one -s 16000 frame ({gen})")

print(f"\n{'OK' if not fails else f'{len(fails)} FAILURES'}")
sys.exit(1 if fails else 0)
