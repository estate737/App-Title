#!/usr/bin/env python3
"""Run the generated style module in official Hermes v0.12.0 (HBC 89).

Requires the user's HomeUI fixture and the external Hermes CLI. Sony bundle
bytes are never included in this repository/package. No PS5 execution claim.
The test bootstraps only the target style module with a minimal require stub.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

ROOT=Path(__file__).resolve().parent.parent
parser=argparse.ArgumentParser()
parser.add_argument('--hermes',required=True,type=Path)
args=parser.parse_args()
hermes=args.hermes.resolve()

def u16(n):return struct.pack('<H',n)
def u32(n):return struct.pack('<I',n)
def new(reg):return bytes([3,reg])
def small(reg,n):return bytes([108,reg,n])
def put(obj,value,sid):return bytes([61,obj,value,sid]) if sid<256 else bytes([62,obj,value])+u16(sid)
def get(dst,obj,cache,sid):return bytes([52,dst,obj,cache,sid]) if sid<256 else bytes([53,dst,obj,cache])+u16(sid)
def closure(dst,env,fn):return bytes([98,dst,env])+u16(fn)
def patch_fn(b,fn,code,params=None):
    h=128+fn*16
    word0,word1=struct.unpack_from('<II',b,h)
    offset=word0&0x1ffffff
    assert not b[h+15]&32
    assert len(code)<=(word1&0x7fff)
    b[offset:offset+len(code)]=code
    struct.pack_into('<I',b,h+4,(word1&~0x7fff)|len(code))
    if params is not None:struct.pack_into('<I',b,h,offset|(params<<25))

for mode in range(8):
    original=(ROOT/f'build-host/home_style_{mode}.hbc').read_bytes()
    assert hashlib.sha1(original[:-20]).digest()==original[-20:]
    b=bytearray(original)
    # identity(arg) used by StyleSheet.create
    patch_fn(b,6676,bytes([106,0,1,90,0]),2)
    # require(1) and require(25): one object supplies only the used exports.
    stub=(new(2)+small(1,12)+put(2,1,5305)+small(1,200)+put(2,1,5800)+
          new(3)+small(1,20)+put(3,1,4494)+put(2,3,27)+
          bytes([50,4])+closure(1,4,6676)+new(3)+put(3,1,107)+put(2,3,47)+bytes([90,2]))
    patch_fn(b,6675,stub,2)
    # Call module(module, exports, require); stringify its style result.
    # Throw is deliberate: capture JSON without relying on extra shell globals.
    boot=(bytes([50,0])+new(1)+closure(2,0,1461)+closure(3,0,6675)+bytes([116,4])+
          bytes([83,4,2,4,4,1,3])+bytes([48,5])+get(6,5,1,33)+get(7,6,2,227)+
          get(1,1,3,110)+bytes([81,7,7,6,1,93,7]))
    patch_fn(b,0,boot)
    # The original global thunk uses only a few registers. Reserve the VM's
    # call-frame area as well as all eight registers used by this test thunk.
    word2=struct.unpack_from('<I',b,128+8)[0]
    struct.pack_into('<I',b,128+8,(word2&0x1ffffff)|(24<<25))
    b[128+13]=3
    b[-20:]=hashlib.sha1(b[:-20]).digest()
    path=ROOT/f'build-host/style_vm_{mode}.hbc';path.write_bytes(b)
    result=subprocess.run([str(hermes),'-b',str(path)],capture_output=True,text=True,timeout=15)
    lines=(result.stdout+result.stderr).splitlines()
    payload=next((line.split('Uncaught ',1)[1] for line in lines if 'Uncaught {' in line),None)
    assert result.returncode==1 and payload,(result.returncode,result.stdout,result.stderr)
    styles=json.loads(payload)
    title=styles.pop('titleId')
    outside=mode<4
    assert styles=={'imageWrapper':{'width':106,'height':202} if outside else {'borderRadius':12},'image':{'width':106,'height':106,'borderRadius':16} if outside else {'width':200,'height':200},'telemetryContainer':{'position':'relative'}},styles
    rgb=0xffffff if mode%4<2 else 0x12abef
    side='top' if mode&1 else 'bottom'
    assert title=={'fontSize':20,'fontWeight':'bold','textAlign':'center','position':'absolute',side:0,'left':0,'right':0,'color':(rgb<<8)|255},title
    (ROOT/f'build-host/style_result_{mode}.json').write_text(json.dumps({'titleId':title,**styles}))
    print(f'PASS: Hermes VM mode={mode} {side} #{rgb:06X}; geometry, native font and remaining styles preserved')
