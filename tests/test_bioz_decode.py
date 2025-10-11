#!/usr/bin/env python3
"""Unit test harness for MAX30001 BioZ decoding.

This mirrors the C decoding implemented in drivers/sensor/max30001/max30001_async.c:
  - 3 bytes per FIFO entry (MSB first)
  - 20-bit left-justified two's complement sample inside bits [23:4]
  - lower 4 bits are tag/status and must be ignored

The test creates several signed 20-bit values, encodes them into bytes, then
applies the Python decoding routine equivalent to the C code and checks we
recover the original signed integers.
"""

import struct


def encode_bioz_sample(signed_20bit):
    """Encode a signed 20-bit value into 3 FIFO bytes (left-justified in 24 bits).

    signed_20bit: integer that fits in signed 20-bit range [-524288, 524287]
    returns: tuple of three bytes (b0, b1, b2)
    """
    if not (- (1 << 19) <= signed_20bit < (1 << 19)):
        raise ValueError("value out of 20-bit signed range")

    # Place the signed value left-justified into bits [23:4]
    # First create the 20-bit two's complement (if negative) as unsigned
    if signed_20bit < 0:
        u20 = (1 << 20) + signed_20bit
    else:
        u20 = signed_20bit

    # left-justify into top 20 bits of 24-bit word: shift left by 4
    word24 = (u20 << 4) & 0xFFFFFF

    # here we could OR in a tag/status nibble in low 4 bits; keep 0x0
    b0 = (word24 >> 16) & 0xFF
    b1 = (word24 >> 8) & 0xFF
    b2 = word24 & 0xFF
    return (b0, b1, b2)


def decode_bioz_bytes(b0, b1, b2):
    """Decode 3 bytes as the driver does: assemble, mask low 4 bits, sign extend."""
    word = (b0 << 16) | (b1 << 8) | b2
    word &= 0xFFFFF0
    # move sign bit to bit31 then arithmetic shift back
    sb = (word << 8) & 0xFFFFFFFF
    if sb & 0x80000000:
        sb = sb - (1 << 32)
    s_bioz = sb >> 12
    return s_bioz


def test_values():
    # test a set of sample values: min, max, negative, small, etc.
    vals = [0, 1, -1, 12345, -12345, (1<<19)-1, -(1<<19), -100000, 100000, 524287, -524288]
    ok = True
    for v in vals:
        try:
            b0,b1,b2 = encode_bioz_sample(v)
        except ValueError:
            print(f"value out of range: {v}")
            ok = False
            continue
        dec = decode_bioz_bytes(b0,b1,b2)
        if dec != v:
            print(f"Mismatch: orig={v}, dec={dec}, bytes={[hex(x) for x in (b0,b1,b2)]}")
            ok = False
        else:
            print(f"OK: {v} -> {[hex(x) for x in (b0,b1,b2)]} -> {dec}")
    return ok


if __name__ == '__main__':
    success = test_values()
    if not success:
        raise SystemExit(1)
    print("All tests passed")
