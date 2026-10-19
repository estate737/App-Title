#!/usr/bin/env python3
"""Independent HBC instruction/branch/cache check; needs external hermes-dec."""
import argparse
import hashlib
import io
from pathlib import Path
import sys

parser=argparse.ArgumentParser()
parser.add_argument('--hermes-dec',required=True,type=Path,help='Path to external hermes-dec checkout')
args=parser.parse_args()
sys.path.insert(0,str(args.hermes_dec.resolve()/'src'))
from hermes_dec.parsers.hbc_file_parser import HBCReader, StringKind
from hermes_dec.parsers.hbc_bytecode_parser import parse_hbc_bytecode

root=Path(__file__).resolve().parent.parent
for mode in range(8):
    data=(root/f'build-host/home_style_{mode}.hbc').read_bytes()
    assert hashlib.sha1(data[:-20]).digest()==data[-20:]
    reader=HBCReader();reader.read_whole_file(io.BytesIO(data))
    for number in (1461,6665,6513,6509,282,6448,1079):
        header=reader.function_headers[number]
        code=list(parse_hbc_bytecode(header,reader))
        assert code[-1].next_pos==header.bytecodeSizeInBytes
        boundaries={ins.original_pos for ins in code}
        cache={'read':{},'write':{}}
        for ins in code:
            for i,operand in enumerate(ins.inst.operands):
                value=getattr(ins,f'arg{i+1}')
                if operand.operand_type.name in ('Addr8','Addr32'):
                    assert ins.original_pos+value in boundaries,(number,ins)
                if operand.operand_type.name.startswith('Reg'):
                    assert value<header.frameSize,(number,ins,header.frameSize)
            if ins.inst.name in ('GetById','GetByIdShort','PutById','PutByIdLong','PutNewOwnById','PutNewOwnByIdShort'):
                sid=getattr(ins,f'arg{len(ins.inst.operands)}')
                assert reader.string_kinds[sid]==StringKind.Identifier,(number,ins,'non-identifier property ID')
            if ins.inst.name in ('GetById','GetByIdShort','PutById'):
                kind='write' if ins.inst.name=='PutById' else 'read'
                index=ins.arg3;sid=ins.arg4
                limit=header.highestWriteCacheIndex if kind=='write' else header.highestReadCacheIndex
                assert index<=limit,(number,ins,limit)
                assert index not in cache[kind] or cache[kind][index]==sid,(number,kind,index,'cache reused for another property')
                cache[kind][index]=sid
        if number==6448:
            by_pos={x.original_pos:x for x in code}
            assert by_pos[0x54b].arg3==(1 if mode<4 else 0)
            assert by_pos[0x559].arg3==(0 if mode<4 else 1)
        assert code[-1].inst.name=='Ret'
        print(f'PASS: independent parser mode={mode} function={number} bytes={header.bytecodeSizeInBytes}; branch boundaries, registers, identifier IDs and distinct caches')
