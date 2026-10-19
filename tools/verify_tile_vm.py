#!/usr/bin/env python3
"""Execute the actual patched tile renderer in official Hermes v89.

Only React hooks/native components are stubbed. Both renderer branches run;
the source HBC fixtures are created by test_home_style and are not shipped.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

ROOT=Path(__file__).resolve().parent.parent
parser=argparse.ArgumentParser();parser.add_argument('--hermes',required=True,type=Path)
hermes=parser.parse_args().hermes.resolve()
def u16(n):return struct.pack('<H',n)
def new(reg):return bytes([3,reg])
def small(reg,n):return bytes([108,reg,n])
def string(reg,sid):return bytes([113,reg])+u16(sid)
def put(obj,value,sid):return bytes([61,obj,value,sid]) if sid<256 else bytes([62,obj,value])+u16(sid)
def get(dst,obj,cache,sid):return bytes([52,dst,obj,cache,sid]) if sid<256 else bytes([53,dst,obj,cache])+u16(sid)
def closure(dst,env,fn):return bytes([98,dst,env])+u16(fn)
def store(env,slot,value):return bytes([42,env,slot,value])
def patch_fn(b,fn,code,params=None):
    h=128+fn*16;word0,word1=struct.unpack_from('<II',b,h)
    offset=word0&0x1ffffff;assert not b[h+15]&32
    assert len(code)<=(word1&0x7fff),(fn,len(code),word1&0x7fff)
    b[offset:offset+len(code)]=code
    struct.pack_into('<I',b,h+4,(word1&~0x7fff)|len(code))
    if params is not None:struct.pack_into('<I',b,h,offset|(params<<25))

for mode in range(8):
    for capture in (False,True):
        b=bytearray((ROOT/f'build-host/home_style_{mode}.hbc').read_bytes())
        assert hashlib.sha1(b[:-20]).digest()==b[-20:]
        patch_fn(b,6657,bytes([116,0,90,0])) # no-op mount hook
        patch_fn(b,6674,bytes([119,0,90,0]),2) # false/error atom
        patch_fn(b,6676,bytes([106,0,1,90,0]),2) # identity callback
        patch_fn(b,6677,new(0)+string(1,188)+put(0,1,7)+bytes([90,0]),2) # source
        patch_fn(b,6678,bytes([106,1,1,106,2,2])+new(0)+put(0,1,203)+put(0,2,200)+bytes([90,0]),3) # jsx(type,props)
        # Stub require for the style module: native font and image constants.
        require=(new(2)+small(1,12)+put(2,1,5305)+small(1,106)+put(2,1,5800)+
                 new(3)+small(1,20)+put(3,1,4494)+put(2,3,27)+bytes([50,4])+
                 closure(1,4,6676)+new(3)+put(3,1,107)+put(2,3,47)+bytes([90,2]))
        patch_fn(b,6675,require,2)
        # Set up exactly the captured module bindings read by function 6665.
        boot=bytearray(bytes([50,0])+new(1)+closure(2,0,1461)+closure(3,0,6675)+bytes([116,4,83,4,2,4,4,1,3])+store(0,11,1))
        for reg,fn in ((10,6657),(11,6676),(12,6674),(13,6677),(14,6678)):
            boot+=closure(reg,0,fn)
        for slot,props in (
            (0,((247,11),)),(1,((244,12),)),(2,((63,13),(245,11))),
            (5,((110,12),)),(6,((8341,10),)),(13,((127,14),))):
            boot+=new(5)
            for sid,reg in props:boot+=put(5,reg,sid)
            if slot==2:boot+=string(6,29)+put(5,6,29)
            boot+=store(0,slot,5)
        boot+=new(5)+string(6,5014)+put(5,6,5014)+string(6,55)+put(5,6,55)
        boot+=new(7)+put(7,6,55)+put(5,7,5)+store(0,3,5)
        boot+=new(5)+put(5,13,10903)+new(6)+put(6,5,4536)+store(0,4,6)
        boot+=new(5)+small(6,168)+put(5,6,7485)+store(0,9,5)
        boot+=new(5)+string(6,237)+put(5,6,110)+store(0,10,5)
        boot+=new(5)+string(6,8854)+put(5,6,110)+store(0,12,5)
        # Actual tile input, including a valid sample title ID and mat style.
        boot+=new(5)+bytes([118 if capture else 119,6])+put(5,6,10417)
        boot+=bytes([115,6])+put(5,6,5048)+new(6)+string(7,188)+put(6,7,7)+put(5,6,10504)
        boot+=new(6)+small(7,1)+put(6,7,184)+put(5,6,9080)
        boot+=bytes([118,6])+put(5,6,8854)+put(5,6,11311)
        boot+=string(6,5492)+put(5,6,237)+string(6,231)+put(5,6,231)
        # Execute twice with the same module shape so inline caches are warm.
        boot+=closure(2,0,6665)+bytes([81,1,2,4,5,81,1,2,4,5,48,5])+get(6,5,1,33)+get(7,6,2,227)+bytes([81,7,7,6,1,93,7])
        patch_fn(b,0,boot)
        word2=struct.unpack_from('<I',b,128+8)[0]
        struct.pack_into('<I',b,128+8,(word2&0x1ffffff)|(48<<25));b[128+12]=15;b[128+13]=3
        b[-20:]=hashlib.sha1(b[:-20]).digest()
        path=ROOT/f'build-host/tile_vm_{mode}_{int(capture)}.hbc';path.write_bytes(b)
        result=subprocess.run([str(hermes),'-b',str(path)],capture_output=True,text=True,timeout=15)
        text=result.stdout+result.stderr
        payload=next((line.split('Uncaught ',1)[1] for line in text.splitlines() if 'Uncaught {' in line),None)
        assert result.returncode==1 and payload,(result.returncode,text)
        tree=json.loads(payload)
        assert tree['type']==('loadComponents' if capture else 'View'),tree
        assert tree['props']['testID']=='testID'
        if mode<4:
            image,title=tree['props']['children']
            assert tree['props']['style']=={'height':202,'width':106}
            assert image['props']['style']==[{'width':106,'height':106,'borderRadius':16},{'marginTop':48}]
            assert 'children' not in image['props']['children']['props']
            assert image['props']['children']['props']['style']==[{'opacity':1},{'width':106,'height':106,'borderRadius':16}]
        else:
            assert tree['props']['style']==[{'borderRadius':12},{'width':106,'height':106}]
            image=tree['props']['children'];title=image['props']['children']['props']['children']
            assert image['props']['style']=={'width':106,'height':106}
        assert image['type']=='Image' and title['type']=='titleId'
        assert title['props']['titleId']=='PPSA01650',title
        (ROOT/f'build-host/tile_result_{mode}_{int(capture)}.json').write_text(json.dumps(tree))
        print(f'PASS: Hermes tile mode={mode} capture={int(capture)} twice; reserved bounds or original inside hierarchy, image/mat and title ID')
