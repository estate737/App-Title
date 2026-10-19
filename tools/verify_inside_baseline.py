#!/usr/bin/env python3
"""Compare complete inside-mode HBC with hashes built from released 0.2.3.

The hashes describe the user's exact HomeUI fixture, not a universal profile.
"""
import hashlib
from pathlib import Path
root=Path(__file__).resolve().parent.parent
expected=[
    'ab430479b7b7913136a6200839fed76171a9439516c002c5d0a41b746fd89bd4',
    'b006d83dc6cc94cc8ccf158917f410a7776be8afddd0ca2ce8b332d4db8b665c',
    '2e78bf0fa0784d4fcc36b4c0d9bedb7f1680d4cd805b1ac94f4b701a33faccc4',
    '4e0255ea13bdaab9adb799faaecfe4f479ec53117de0b22f6691562e58df4f58',
]
for i,digest in enumerate(expected):
    data=(root/f'build-host/home_style_{i+4}.hbc').read_bytes()
    assert hashlib.sha256(data).hexdigest()==digest,i
print('PASS: all four inside-mode complete HomeUI outputs match released 0.2.3 byte for byte')
