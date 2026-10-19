// SPDX-License-Identifier: GPL-3.0-or-later
// Layout using styles/trees captured from the real patched functions in Hermes.
const assert=require('assert'),fs=require('fs'),path=require('path');
const {loadYoga}=require('../build-ui/node_modules/yoga-layout/src/entrypoint/wasm-async-node.js');
const read=n=>JSON.parse(fs.readFileSync(path.join(__dirname,'../build-host/'+n)));
(async()=>{
 const Yoga=await loadYoga();let cases=0;
 for(let mode=0;mode<4;mode++){
  const titleStyle=read(`style_result_${mode}.json`).titleId;
  const tree=read(`tile_result_${mode}_0.json`),geometry=read(`geometry_result_${mode}.json`);
  const [bitmap,title]=tree.props.children;assert.equal(title.type,'titleId');
  const frameStyle=tree.props.style,imageStyle=Object.assign({},...bitmap.props.style);
  assert.equal(frameStyle.height,geometry['6513'].tile.height);
  const gameTitle=geometry['1079'];
  assert.equal(gameTitle.TITLE_Y-frameStyle.height,12);
  assert.equal(gameTitle.default.experienceTitleContainer.top,gameTitle.TITLE_Y);
  assert(gameTitle.TITLE_Y+gameTitle.default.itemContainer.height<=geometry['282'].default.container.height);
  assert(gameTitle.TITLE_X>=geometry['282'].SCALED_EXP_MARGIN_LEFT+geometry['282'].SCALED_EXP_SIZE+16);
  for(const line of [20,28,40])for(const clip of [false,true]){
   const frame=Yoga.Node.create(),image=Yoga.Node.create(),label=Yoga.Node.create();
   frame.setWidth(frameStyle.width);frame.setHeight(frameStyle.height);frame.setOverflow(clip?Yoga.OVERFLOW_HIDDEN:Yoga.OVERFLOW_VISIBLE);
   image.setWidth(imageStyle.width);image.setHeight(imageStyle.height);image.setMargin(Yoga.EDGE_TOP,imageStyle.marginTop);
   frame.insertChild(image,0);frame.insertChild(label,1);
   label.setPositionType(Yoga.POSITION_TYPE_ABSOLUTE);label.setPosition(Yoga.EDGE_LEFT,titleStyle.left);label.setPosition(Yoga.EDGE_RIGHT,titleStyle.right);
   label.setMeasureFunc(()=>({width:96,height:line}));
   if(titleStyle.top!==undefined)label.setPosition(Yoga.EDGE_TOP,titleStyle.top);
   if(titleStyle.bottom!==undefined)label.setPosition(Yoga.EDGE_BOTTOM,titleStyle.bottom);
   frame.calculateLayout(undefined,undefined,Yoga.DIRECTION_LTR);
   const text=label.getComputedLayout(),picture=image.getComputedLayout();
   assert.equal(picture.width,106);assert.equal(picture.height,106);assert.equal(picture.top,48);
   assert(text.top>=0&&text.top+text.height<=frameStyle.height);
   if(titleStyle.top!==undefined)assert(text.top+text.height<picture.top);else assert(text.top>picture.top+picture.height);
   // Check bounds after selected scale/translateY in the actual animation
   // builder. RN transform order applies the translate after the scale matrix.
   const scale=geometry['282'].EXPERIENCE_SCALE,half=frameStyle.height/2;
   const ty=geometry['6509'].selected0.transform[1].translateY;
   const worldTop=geometry['6513'].container.marginTop+half-scale*half+scale*ty;
   assert(Math.abs(worldTop)<.001);assert(worldTop+scale*frameStyle.height<=geometry['282'].default.strandContainer.height);
   assert.equal(geometry['282'].default.container.height,336);
   frame.freeRecursive();cases++;
  }
 }
 console.log(`PASS: Yoga ${cases} layouts; auto-height ID inside reserved positive bounds, separate from unchanged bitmap, selected bounds fit expanded row (host model, not RNPS validation)`);
})().catch(e=>{console.error(e);process.exitCode=1;});
