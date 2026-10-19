#!/usr/bin/env python3
"""Execute original/current parent renderers in Hermes and compare their trees.

Only React hooks, imported modules and native components are mocked. The real
1716-byte function runs twice per case. A test-only Function constructor
bootstraps our mocks in an unused translation string in the local HBC copy.
No source fixture, Sony string or bytecode is included in the release archive.
"""
import argparse
import hashlib
import itertools
import json
from pathlib import Path
import struct
import subprocess

ROOT=Path(__file__).resolve().parent.parent
p=argparse.ArgumentParser();p.add_argument('--hermes',required=True,type=Path);p.add_argument('--home-ui',required=True,type=Path)
args=p.parse_args();hermes=args.hermes.resolve()
bundle=args.home_ui.read_bytes();start=struct.unpack_from('<I',bundle,0x1c)[0]
original=bundle[start:start+struct.unpack_from('<I',bundle,0x24)[0]]
assert hashlib.sha1(original[:-20]).digest()==original[-20:]==bytes.fromhex('3dacc31c204c037cb7922f3bf863cf962b23497e')

def u16(n):return struct.pack('<H',n)
def get(dst,obj,cache,sid):return bytes([52,dst,obj,cache,sid]) if sid<256 else bytes([53,dst,obj,cache])+u16(sid)
def patch_fn(b,number,code):
    h=128+16*number;word0,word1=struct.unpack_from('<II',b,h);offset=word0&0x1ffffff
    assert len(code)<=(word1&0x7fff)
    b[offset:offset+len(code)]=code;struct.pack_into('<I',b,h+4,(word1&~0x7fff)|len(code))
def mock_string(b,source):
    # Discover the existing storage location from v89 table metadata.
    fn,kinds,ids,strings,overflow=struct.unpack_from('<IIIII',b,40)
    table=128+16*fn+4*kinds+4*ids;word=struct.unpack_from('<I',b,table+4307*4)[0]
    assert word&1 and word>>24==255
    large=table+strings*4;offset,length=struct.unpack_from('<II',b,large+((word>>1)&0x7fffff)*8)
    storage=large+overflow*8;assert length>=len(source)
    encoded=source.ljust(length).encode('utf-16-le');assert len(encoded)==length*2
    b[storage+offset:storage+offset+len(encoded)]=encoded

source=r'''
var DL=__DL__, A=__A__, IDX=__IDX__;
var noop=function(){}, identity=function(x){return x;};
var jsx=function(type,props){return {type:type,props:props};};
var atom=function(x){return x;};
var react={useRef:function(x){return {current:x};},useMemo:function(){return 'memo';},useCallback:identity,Suspense:'Suspense'};
var options={closeOptionsMenu:noop,onOptionsKeyDown:noop,setHasDisc:noop,experienceOptionsMenu:[],optionsMenuItemsCount:0,hasDisc:false};
var data={showDownloadBar:DL,platformType:'PS5',packageType:'FULL',platformIconId:null};
var env=[
 {default:identity}, {}, {STRING_ID:{FULL:'FULL'}}, {useAtomValue:atom},react,
 {Animated:{View:'Animated.View'},View:'View'}, {FocusLayerPS:'FocusLayerPS'},
 {default:A}, {default:function(){return data;}}, {default:{experienceId:'sample'}},
 {useLink:function(){return 'link';}}, {usePerformanceMount:noop},
 {default:{focusContainer:{borderRadius:25.358490566037737},downloadbarContainer:{top:'100%',position:'absolute',left:0,right:0,marginTop:2,alignItems:'center'},downloadbar:{width:90}}},
 {default:'Tile'}, {default:{shim:{height:106,width:106}}},
 {default:function(){return {barFadeStyle:{opacity:1},onDownloadStateUpdate:noop};}},
 {useKeyHandlers:function(){return {onKeyDown:noop};}},
 {useTileOptionsMenu:function(){return options;}}, {default:'PlatformIcon'},
 {jsx:jsx,jsxs:jsx}, 'DownloadProgressBar',noop
];
var experience={id:'sample',experienceName:'Sample game',fallbackIconPath:{uri:'fallback'},hubUri:'hub',iconPath:{uri:'icon'},linkUri:'link',pinnedStatus:false,titleId:'PPSA01650'};
var props={LazyTileOptionsMenu:'Options',experience:experience,experiences:[experience,experience],experiencesCount:2,getCurrentFocus:noop,
 barContainerStyle:{barOpacity0:{opacity:1}},headerTransitionStylesheet:{},index:IDX,loadComponents:true,
 matStyle:{},pinLimitReached:false,poolIndex:0,onItemPress:function(){return noop;},onFocusMove:noop,setOptionsMenuStatus:noop,
 spaceId:'games',startupStyles:{},userId:1,usersSpaceText:'Games'};
props.headerTransitionStylesheet['experienceOpacity'+0]={opacity:1};
props.headerTransitionStylesheet['minimizeExperience'+0]={transform:[{scale:1}]};
props.startupStyles['experience'+IDX]={transform:[{scale:1}]};props.matStyle['mat'+IDX]={opacity:1};
return {t:env,value:props};
'''

cases=0
for mode,download,accordion,index in itertools.product(range(8),(False,True),(False,True),(0,1)):
    fixture=(ROOT/f'build-host/home_style_{mode}.hbc').read_bytes()
    trees=[]
    for patched in (False,True):
        b=bytearray(fixture)
        if not patched:
            h=128+16*6517;b[h:h+16]=original[h:h+16];b[0x156799:0x156799+1716]=original[0x156799:0x156799+1716]
        js=source.replace('__DL__',str(download).lower()).replace('__A__',str(accordion).lower()).replace('__IDX__',str(index))
        mock_string(b,js)
        boot=bytearray([50,0,48,1])
        boot+=get(2,1,1,4528)+bytes([113,3])+u16(4307)+bytes([116,4,81,2,2,4,3,79,3,2,4])
        boot+=get(5,3,2,2)
        for slot in range(22):boot+=bytes([108,6,slot,71,7,5,6,42,0,slot,7])
        boot+=get(5,3,3,188)+bytes([98,2,0])+u16(6517)
        boot+=bytes([82,7,2,4,5,4,82,7,2,4,5,4])
        boot+=get(6,1,4,33)+get(2,6,5,227)+bytes([81,7,2,6,7,93,7])
        patch_fn(b,0,boot)
        meta=struct.unpack_from('<I',b,128+8)[0];struct.pack_into('<I',b,128+8,(meta&0x1ffffff)|(32<<25))
        b[128+12]=22;b[128+13]=5;b[-20:]=hashlib.sha1(b[:-20]).digest()
        path=ROOT/'build-host/focus_vm.hbc';path.write_bytes(b)
        result=subprocess.run([str(hermes),'-b',str(path)],capture_output=True,text=True,timeout=15)
        lines=(result.stdout+result.stderr).splitlines()
        payload=next((x.split('Uncaught ',1)[1] for x in lines if 'Uncaught {' in x),None)
        assert result.returncode==1 and payload,(mode,download,accordion,index,patched,result.returncode,result.stdout,result.stderr)
        tree=json.loads(payload);assert tree['type']=='FocusLayerPS'
        focus=tree['props']['children'][0];assert focus['type']=='Animated.View'
        assert focus['props']['focusable'] is True and focus['props']['name']=='sample'
        assert 'ignoreBorderRadiusClipping' not in focus['props']
        trees.append(tree)
        if mode==0 and not download and not accordion and not index:(ROOT/'build-host/focus_result.json').write_text(json.dumps(tree))
    assert trees[0]==trees[1],(mode,download,accordion,index,'unintended parent render change')
    cases+=1
print(f'PASS: Hermes parent {cases} cases, original vs patched, twice each; original parent function retained without clipping override; focus/accessibility/children/download branches preserved')
