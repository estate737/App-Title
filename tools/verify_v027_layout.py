#!/usr/bin/env python3
"""Prove all working outside-ID bytecode remains identical to released 0.2.7.

Restoring only the original game-title function must reproduce the entire
previous HBC hash. Requires the same external user-provided HomeUI fixture.
"""
import argparse
import hashlib
from pathlib import Path
import struct

p=argparse.ArgumentParser();p.add_argument('--home-ui',required=True,type=Path)
bundle=p.parse_args().home_ui.read_bytes()
start=struct.unpack_from('<I',bundle,0x1c)[0];plain=struct.unpack_from('<I',bundle,0x24)[0]
original=bundle[start:start+plain]
assert hashlib.sha1(original[:-20]).digest()==original[-20:]==bytes.fromhex('3dacc31c204c037cb7922f3bf863cf962b23497e')
expected=[
    'adb2935e01df9b6329c31f32de1e0e1c46460a402167956a677f9532c48a6b02',
    'fdc7129f3a9227171c1f2c49832fb3b29e04abc0e184b1c8eb849de03fb384f5',
    '46fcfec829537727aa98573ecd70ad0865957a7e06f784e59e659d9b7dcc4330',
    '0d7663c5f5c0c0c8aa7c3f06eada300e1c4e12b0dfd5f07db148ff4ea12e603a',
]
root=Path(__file__).resolve().parent.parent
for i,digest in enumerate(expected):
    b=bytearray((root/f'build-host/home_style_{i}.hbc').read_bytes())
    h=128+16*1079;b[h:h+16]=original[h:h+16]
    b[0xdaff4:0xdaff4+361]=original[0xdaff4:0xdaff4+361]
    b[-20:]=hashlib.sha1(b[:-20]).digest()
    assert hashlib.sha256(b).hexdigest()==digest,i
print('PASS: all four outside modes differ from working 0.2.7 only in game-title function, its header and checksum')
