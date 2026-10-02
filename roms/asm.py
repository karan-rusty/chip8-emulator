#!/usr/bin/env python3
"""Assemble roms/snake.asm -> roms/snake.ch8 (original CHIP-8 only)."""
import re
import sys
from pathlib import Path

SRC = Path(__file__).with_name("snake.asm")
OUT = Path(__file__).with_name("snake.ch8")
BASE = 0x200


def num(tok):
    tok = tok.strip()
    if tok.lower().startswith("0x"):
        return int(tok, 16)
    return int(tok, 10)


def reg(tok):
    tok = tok.strip().upper()
    assert re.fullmatch(r"V[0-9A-F]", tok), f"bad register {tok}"
    return int(tok[1], 16)


def split_args(s):
    return [a.strip() for a in s.split(",") if a.strip() != ""]


def addr_of(operand, labels):
    operand = operand.strip()
    m = re.fullmatch(r"([A-Za-z_][\w]*)(?:\s*\+\s*(.+))?", operand)
    assert m, f"bad address {operand}"
    name, off = m.group(1).upper(), m.group(2)
    assert name in labels, f"unknown label {name}"
    v = labels[name]
    if off is not None:
        v += num(off)
    assert 0 <= v <= 0xFFF, f"address out of range {v:#x}"
    return v


def first_pass(lines):
    labels = {}
    items = []  # (kind, payload, addr)
    pc = BASE
    for ln, raw in enumerate(lines, 1):
        line = raw.split(";")[0].strip()
        if not line:
            continue
        while True:
            m = re.match(r"([A-Za-z_][\w]*)\s*:\s*(.*)$", line)
            if not m:
                break
            name = m.group(1).upper()
            assert name not in labels, f"dup label {name} (line {ln})"
            labels[name] = pc
            line = m.group(2).strip()
            if not line:
                break
        if not line:
            continue
        up = line.upper()
        if up.startswith("DB "):
            vals = [num(t) for t in split_args(line[3:])]
            for v in vals:
                assert 0 <= v <= 0xFF
            items.append(("db", vals, pc))
            pc += len(vals)
        elif up.startswith("DS "):
            n = num(line[3:].strip())
            items.append(("ds", n, pc))
            pc += n
        else:
            parts = line.split(None, 1)
            op = parts[0].upper()
            args = split_args(parts[1]) if len(parts) > 1 else []
            items.append(("op", (op, args, ln, line), pc))
            pc += 2
    assert pc - BASE <= 3584, f"ROM too big: {pc - BASE} bytes"
    return labels, items


def emit(op, args, labels):
    if op in ("CLS", "RET"):
        return {"CLS": 0x00E0, "RET": 0x00EE}[op]
    if op in ("JP", "CALL", "SYS"):
        base = {"JP": 0x1000, "CALL": 0x2000, "SYS": 0x0000}[op]
        a = args[0]
        if a.upper().startswith("V0,"):
            assert op == "JP"
            return 0xB000 | addr_of(a[3:], labels)
        return base | addr_of(a, labels)
    if op in ("SE", "SNE"):
        assert len(args) == 2
        x = reg(args[0])
        if re.fullmatch(r"V[0-9A-Fa-f]", args[1].strip()):
            y = reg(args[1])
            assert op == "SE" and False or True
            return (0x5000 if op == "SE" else 0x9000) | (x << 8) | (y << 4)
        return (0x3000 if op == "SE" else 0x4000) | (x << 8) | num(args[1])
    if op == "LD":
        assert len(args) == 2
        d, s = args[0].strip(), args[1].strip()
        du, su = d.upper(), s.upper()
        if du == "I":
            return 0xA000 | addr_of(s, labels)
        if du == "[I]":
            return 0xF055 | (reg(s) << 8)
        if su == "[I]":
            return 0xF065 | (reg(d) << 8)
        if su == "DT":
            return 0xF007 | (reg(d) << 8)
        if su == "K":
            return 0xF00A | (reg(d) << 8)
        if du == "DT":
            return 0xF015 | (reg(s) << 8)
        if du == "ST":
            return 0xF018 | (reg(s) << 8)
        if du == "F":
            return 0xF029 | (reg(s) << 8)
        if du == "B":
            return 0xF033 | (reg(s) << 8)
        if re.fullmatch(r"V[0-9A-Fa-f]", s):
            return 0x8000 | (reg(d) << 8) | (reg(s) << 4)
        return 0x6000 | (reg(d) << 8) | num(s)
    if op == "ADD":
        assert len(args) == 2
        d, s = args[0].strip(), args[1].strip()
        if d.upper() == "I":
            return 0xF01E | (reg(s) << 8)
        if re.fullmatch(r"V[0-9A-Fa-f]", s):
            return 0x8004 | (reg(d) << 8) | (reg(s) << 4)
        return 0x7000 | (reg(d) << 8) | num(s)
    if op in ("OR", "AND", "XOR", "SUB", "SUBN"):
        n = {"OR": 1, "AND": 2, "XOR": 3, "SUB": 5, "SUBN": 7}[op]
        return 0x8000 | (reg(args[0]) << 8) | (reg(args[1]) << 4) | n
    if op in ("SHR", "SHL"):
        n = 6 if op == "SHR" else 0xE
        if len(args) == 1:
            x = reg(args[0])
            return 0x8000 | (x << 8) | (x << 4) | n
        return 0x8000 | (reg(args[0]) << 8) | (reg(args[1]) << 4) | n
    if op == "RND":
        return 0xC000 | (reg(args[0]) << 8) | num(args[1])
    if op == "DRW":
        return 0xD000 | (reg(args[0]) << 8) | (reg(args[1]) << 4) | num(args[2])
    if op in ("SKP", "SKNP"):
        return (0xE09E if op == "SKP" else 0xE0A1) | (reg(args[0]) << 8)
    raise AssertionError(f"unknown op {op}")


def main():
    src = Path(sys.argv[1]) if len(sys.argv) > 1 else SRC
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else src.with_suffix(".ch8")
    lines = src.read_text().splitlines()
    labels, items = first_pass(lines)
    rom = bytearray()
    for kind, pay, addr in items:
        if kind == "db":
            rom.extend(pay)
        elif kind == "ds":
            rom.extend(b"\x00" * pay)
        else:
            op, args, ln, src = pay
            try:
                w = emit(op, args, labels)
            except AssertionError as e:
                sys.exit(f"line {ln}: {src}\n  {e}")
            rom += bytes([w >> 8, w & 0xFF])
    out.write_bytes(bytes(rom))
    print(f"{out.name}: {len(rom)} bytes, labels:")
    for k in ("INIT", "MAIN", "INPUT", "SPAWN", "DRAW", "DEAD",
              "SPR", "BODY", "TMP", "SCORE"):
        if k in labels:
            print(f"  {k:<8} {labels[k]:#05x}")


if __name__ == "__main__":
    main()
