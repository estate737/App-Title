#!/usr/bin/env python3
"""Execute real row, tile geometry and animation builders in Hermes v89.

Only imported RN styles/animation values are mocked. Input HBC stays local.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

ROOT=Path(__file__).resolve().parent.parent
p=argparse.ArgumentParser();p.add_argument('--hermes',required=True,type=Path)
hermes=p.parse_args().hermes.resolve()
def u16(n):return struct.pack('<H',n)
def get(dst,obj,cache,sid):return bytes([52,dst,obj,cache,sid]) if sid<256 else bytes([53,dst,obj,cache])+u16(sid)
def patch_fn(b,number,code):
    h=128+16*number;word0,word1=struct.unpack_from('<II',b,h);offset=word0&0x1ffffff
    assert len(code)<=(word1&0x7fff)
    b[offset:offset+len(code)]=code;struct.pack_into('<I',b,h+4,(word1&~0x7fff)|len(code))
def mock_string(b,source):
    fn,kinds,ids,strings,overflow=struct.unpack_from('<IIIII',b,40)
    table=128+16*fn+4*kinds+4*ids;word=struct.unpack_from('<I',b,table+4307*4)[0]
    assert word&1 and word>>24==255
    large=table+strings*4;offset,length=struct.unpack_from('<II',b,large+((word>>1)&0x7fffff)*8)
    storage=large+overflow*8;assert length>=len(source)
    encoded=source.ljust(length).encode('utf-16-le');assert len(encoded)==length*2
    b[storage+offset:storage+offset+len(encoded)]=encoded

source=r'''
var native={StyleSheet:{create:function(x){return x;}},FontSizePS:{SizeNormal:20},EXPERIENCE_SIZE:106,SCALED_EXP_SIZE:168,SCALED_EXP_MARGIN_LEFT:172};
var props={itemSize:__SIZE__,itemMargin:8,maxItems:2,offsets:[0,114],positions:[0,114],sizes:[1,168/106],selectedValue:{interpolate:function(x){return x;}}};
return {t:native,value:props,source:function(){return native;}};
'''
cases=0
for mode in range(8):
    results={}
    for size in (80,106,168):
        for number in (6513,6509,282,1079):
            b=bytearray((ROOT/f'build-host/home_style_{mode}.hbc').read_bytes())
            mock_string(b,source.replace('__SIZE__',str(size)))
            boot=bytearray([50,0,48,1])
            boot+=get(2,1,1,4528)+bytes([113,3])+u16(4307)+bytes([116,4,81,2,2,4,3,79,3,2,4])
            boot+=get(5,3,2,2)+bytes([42,0,0,5])+get(5,3,3,188)
            boot+=bytes([98,2,0])+u16(number)
            if number in (282,1079):
                boot+=get(6,3,4,218)+bytes([3,5,83,7,2,4,4,5,6,83,7,2,4,4,5,6,8,7,5])
            else:boot+=bytes([81,7,2,4,5,81,7,2,4,5])
            boot+=get(6,1,5,33)+get(2,6,6,227)+bytes([81,7,2,6,7,93,7])
            patch_fn(b,0,boot)
            meta=struct.unpack_from('<I',b,128+8)[0];struct.pack_into('<I',b,128+8,(meta&0x1ffffff)|(32<<25))
            b[128+12]=1;b[128+13]=6;b[-20:]=hashlib.sha1(b[:-20]).digest()
            path=ROOT/'build-host/geometry_vm.hbc';path.write_bytes(b)
            r=subprocess.run([str(hermes),'-b',str(path)],capture_output=True,text=True,timeout=15)
            payload=next((x.split('Uncaught ',1)[1] for x in (r.stdout+r.stderr).splitlines() if 'Uncaught {' in x),None)
            assert r.returncode==1 and payload,(mode,size,number,r.returncode,r.stdout,r.stderr)
            value=json.loads(payload);outside=mode<4;full=size+(96 if outside else 0)
            if number==6513:
                assert value=={'tile':{'position':'absolute','width':size,'height':full},'container':{'marginTop':-full/2}},value
            elif number==6509:
                for i in (0,1):
                    assert value['selected'+str(i)]['transform']==[{'scale':[1,168/106][i]},{'translateY':full/2},{'translateX':[0,114][i]}],value
                    assert value['position'+str(i)]=={'transform':[{'translateX':[0,114][i]}]},value
            elif number==282:
                assert value['EXPERIENCE_SIZE']==106 and value['SCALED_EXP_SIZE']==168 and value['EXPERIENCE_SCALE']==168/106,value
                assert value['default']['container']['height']==(336 if outside else 168),value
                assert value['default']['strandContainer']['height']==(336 if outside else 168),value
            else:
                assert value['TITLE_Y']==(214 if outside else 106),value
                assert value['TITLE_X']==356 and value['TITLE_MARGIN_LEFT']==16,value
                assert value['MINIMIZED_TITLE_MARGIN_LEFT']==44 and value['MINIMIZED_TITLE_MARGIN_TOP']==9,value
                assert value['default']['experienceTitleContainer']=={'position':'absolute','left':356,'top':214 if outside else 106},value
                assert value['default']['itemContainer']['height']==62,value
            if size==106:results[str(number)]=value
            cases+=1
    (ROOT/f'build-host/geometry_result_{mode}.json').write_text(json.dumps(results))
    print(f'PASS: Hermes geometry mode={mode}, 3 dynamic sizes, row/geometry/animation/game title twice each')
print(f'PASS: {cases} real builder cases; dimensions, centering, scale and original constants')
