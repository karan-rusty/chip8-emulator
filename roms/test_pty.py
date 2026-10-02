#!/usr/bin/env python3
"""Drive the real ./build/chip8 + roms/snake.ch8 through a PTY and assert
visible behavior: snake+food pixels, WASD turns, no arrow-key phantom input,
death screen with score, restart on keypress.

Usage: python3 roms/test_pty.py [--keep-going]
"""
import codecs
import os
import pty
import re
import select
import struct
import subprocess
import sys
import termios
import time
import fcntl

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = os.path.join(ROOT, "build", "chip8")
ROM = os.path.join(ROOT, "roms", "snake.ch8")

fails = []


def check(cond, msg):
    print(("PASS " if cond else "FAIL ") + msg, flush=True)
    if not cond:
        fails.append(msg)


def spawn(args):
    master, slave = pty.openpty()
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 150, 0, 0))
    p = subprocess.Popen(
        args, stdin=slave, stdout=slave, stderr=subprocess.PIPE,
        cwd=ROOT, close_fds=True)
    os.close(slave)
    return master, p


def drain(master, secs):
    """Read everything available for `secs` seconds, return text."""
    dec = codecs.getincrementaldecoder("utf-8")(errors="replace")
    out = []
    end = time.time() + secs
    while time.time() < end:
        r, _, _ = select.select([master], [], [], 0.05)
        if r:
            try:
                chunk = os.read(master, 65536)
            except OSError:
                break
            if not chunk:
                break
            out.append(dec.decode(chunk))
    out.append(dec.decode(b"", final=True))
    return "".join(out)


CELL = {"  ": (0, 0), "\u2588\u2588": (1, 1),
        "\u2580\u2580": (1, 0), "\u2584\u2584": (0, 1)}


def parse_frames(text):
    """Return list of 64x32 framebuffers (list of 2048 bools)."""
    frames = []
    for chunk in text.split("\x1b[H"):
        lines = re.findall(r"\[92m(.{128})", chunk)
        if len(lines) < 16:
            continue
        fb = [False] * (64 * 32)
        for ry, line in enumerate(lines[:16]):
            for cx in range(64):
                cell = line[2 * cx:2 * cx + 2]
                if cell not in CELL:
                    break
                top, bot = CELL[cell]
                if top:
                    fb[(2 * ry) * 64 + cx] = True
                if bot:
                    fb[(2 * ry + 1) * 64 + cx] = True
            else:
                continue
            break
        frames.append(fb)
    return frames


def lit(fb):
    return sum(1 for p in fb if p)


def bbox(fb):
    xs = [i % 64 for i, p in enumerate(fb) if p]
    ys = [i // 64 for i, p in enumerate(fb) if p]
    if not xs:
        return None
    return (min(xs), max(xs), min(ys), max(ys))


def snake_bbox(fb):
    """BBox of the most snake-like blob (long, not the compact 4x4 food)."""
    seen = bytearray(64 * 32)
    best = None  # (width+height, area, bbox)
    for i, p in enumerate(fb):
        if not p or seen[i]:
            continue
        stack = [i]
        seen[i] = 1
        xs, ys = [], []
        while stack:
            j = stack.pop()
            x, y = j % 64, j // 64
            xs.append(x)
            ys.append(y)
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    if not dx and not dy:
                        continue
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < 64 and 0 <= ny < 32:
                        k = ny * 64 + nx
                        if fb[k] and not seen[k]:
                            seen[k] = 1
                            stack.append(k)
        bb = (min(xs), max(xs), min(ys), max(ys))
        score = (bb[1] - bb[0] + bb[3] - bb[2], len(xs), bb)
        if best is None or score[:2] > best[:2]:
            best = score
    return best[2] if best else None


def main():
    for f in (BIN, ROM):
        if not os.path.exists(f):
            print(f"missing {f}; build first")
            sys.exit(2)
    master, p = spawn([BIN, ROM, "-s", "120", "-q", "modern"])

    # --- A: snake + food visible ---
    text = drain(master, 1.5)
    frames = parse_frames(text)
    check(len(frames) > 10, f"frames flowing ({len(frames)} in 1.5s)")
    fb = frames[-1] if frames else [False] * 2048
    n = lit(fb)
    # snake 4x(2x2)=16px + food 2x2=4px = 20 (allow margin)
    check(12 <= n <= 80, f"snake+food pixels visible (lit={n})")
    bb = bbox(fb)
    check(bb is not None and bb[1] - bb[0] >= 6,
          f"snake is a horizontal body (bbox={bb})")

    # --- B: 's' turns down (early: plenty of room below) ---
    os.write(master, b"s")
    text = drain(master, 2.0)
    frames = parse_frames(text)
    first, last = snake_bbox(frames[0]), snake_bbox(frames[-1])
    check(last is not None and first is not None and last[3] > first[3] + 1,
          f"'s' steers down (snake ymax {first[3] if first else '?'}"
          f"->{last[3] if last else '?'})")

    # --- C: uppercase 'A' turns left (fold check; down->left is legal) ---
    os.write(master, b"A")
    text = drain(master, 2.0)
    frames = parse_frames(text)
    first, last = snake_bbox(frames[0]), snake_bbox(frames[-1])
    check(last is not None and first is not None and last[0] < first[0],
          f"uppercase 'A' steers left (snake xmin {first[0] if first else '?'}"
          f"->{last[0] if last else '?'})")

    # --- D: arrow keys must not inject phantom input ---
    text = drain(master, 0.3)
    first = snake_bbox(parse_frames(text)[-1])
    for _ in range(3):
        os.write(master, b"\x1b[A")
        time.sleep(0.2)
    text = drain(master, 1.5)
    frames = parse_frames(text)
    last = snake_bbox(frames[-1]) if frames else None
    dfb = frames[-1] if frames else []
    alive = not (dfb and dfb[10 * 64 + 18] and dfb[10 * 64 + 46])
    ok = (alive and first is not None and last is not None
          and last[0] <= first[0])
    check(ok, f"arrow keys cause no veer and game alive "
              f"(snake bbox {first}->{last})")

    # --- E: steer into the floor for the death screen ---
    os.write(master, b"s")  # ensure heading down
    dead_text = ""
    for _ in range(30):
        t = drain(master, 1.0)
        dead_text += t
        if "\a" in t:  # ST beep on death
            break
    check("\a" in dead_text, "death beeps (BEL on game over)")
    text = drain(master, 2.0)
    frames = parse_frames(text)
    frozen = (len(frames) >= 2 and all(f == frames[-1] for f in frames[-2:])
              and lit(frames[-1]) > 0)
    check(frozen, "death screen holds a frame (score shown, waiting for key)")
    dfb = frames[-1]
    border = (dfb[10 * 64 + 18] and dfb[10 * 64 + 46]
              and dfb[22 * 64 + 18] and dfb[22 * 64 + 46])
    check(border, "death screen shows the border panel")
    os.write(master, b"x")
    text = drain(master, 3.0)
    frames = parse_frames(text)
    bb = bbox(frames[-1]) if frames else None
    restarted = (bb is not None and 12 <= lit(frames[-1]) <= 80
                 and bb[1] - bb[0] >= 6)
    check(restarted, f"any key restarts the game (bbox={bb})")

    try:
        os.write(master, b"\x03")
    except OSError:
        pass
    try:
        p.wait(timeout=5)
    except subprocess.TimeoutExpired:
        p.kill()
    os.close(master)
    print("exit code:", p.returncode)
    print("BELS heard:", dead_text.count("\a"))
    print(f"\n{'OK' if not fails else str(len(fails)) + ' FAILURES'}")
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
