"use strict";
(function(root,factory){const api=factory();if(typeof module==="object"&&module.exports)module.exports=api;else root.CircuitDeviceView=api;}(globalThis,()=>{
  const types=new Set(['FramebufferDisplay32','ButtonDevice32','ConsoleDevice32','TimerDevice32','SynchronousFifo<8>','SynchronousFifo<32>']);
  const images=new WeakMap();
  function supports(type){return types.has(type);}
  function number(bits){return typeof bits==='string'&&/^[01]+$/.test(bits)?parseInt(bits,2):null;}
  function validKeyboardState(state){
    const bits=(value,width)=>typeof value==='string'&&value.length===width&&/^[01XZ]+$/.test(value);
    return state?.supported===true&&state.kind==='keyboard'&&bits(state.levels,32)
      &&bits(state.count,3)&&bits(state.head,32)&&bits(state.overflow,1);
  }
  function keyboard(state){
    const bits=typeof state?.levels==='string'?state.levels.padStart(32,'X'):'X'.repeat(32);
    return Array.from({length:32},(_,id)=>({id,value:bits[31-id],label:String(id)}));
  }
  function rgba(frame){
    if(!Number.isInteger(frame.width)||!Number.isInteger(frame.height)||!Number.isInteger(frame.stride)
      ||frame.width<1||frame.height<1||frame.width>4096||frame.height>4096||frame.stride<frame.width||frame.stride*frame.height>262144
      ||frame.format!=='rgb332'||typeof frame.pixels!=='string'||typeof frame.known!=='string'
      ||frame.pixels.length!==2*frame.stride*frame.height||frame.known.length!==frame.pixels.length
      ||!/^[a-f0-9]*$/i.test(frame.pixels+frame.known))throw new Error('Invalid device image');
    const result=new Uint8ClampedArray(frame.width*frame.height*4);
    for(let y=0;y<frame.height;++y)for(let x=0;x<frame.width;++x){
      const offset=2*(y*frame.stride+x),value=parseInt(frame.pixels.slice(offset,offset+2),16),known=parseInt(frame.known.slice(offset,offset+2),16),p=4*(y*frame.width+x);
      if(known!==255){result[p]=255;result[p+1]=((x+y)&1)?0:60;result[p+2]=255;}
      else {const r=value>>5,g=(value>>2)&7,b=value&3;result[p]=(r<<5)|(r<<2)|(r>>1);result[p+1]=(g<<5)|(g<<2)|(g>>1);result[p+2]=b*85;}
      result[p+3]=255;
    }
    return result;
  }
  function summary(state){
    if(!state?.supported)return state?.reason||'Device state pending';
    const count=number(state.count),head=number(state.head);
    if(state.kind==='display')return `${state.width} × ${state.height} RGB332 · sequence ${number(state.sequence)??'X'}`;
    if(state.kind==='keyboard')return `Held: ${keyboard(state).filter(k=>k.value==='1').map(k=>k.id).join(', ')||'none'} · queue ${count??'X'} · head ${head??'X'} · overflow ${state.overflow}`;
    if(state.kind==='console'){const value=number(state.data);return `TX ${state.valid} · byte ${value??'X'}${value!==null&&value>=32&&value<=126?` (${String.fromCharCode(value)})`:''} · queue ${count??'X'}`;}
    return `${state.kind==='timer'?'Ticks':'Queue'} ${count??'X'}${state.kind==='queue'?` · head ${head??'X'} · overflow ${state.overflow}`:''}`;
  }
  function draw(ctx,rect,state){
    if(rect.w<40||rect.h<32)return;
    ctx.save();ctx.beginPath();ctx.rect(rect.x,rect.y,rect.w,rect.h);ctx.clip();
    ctx.fillStyle='#121923';ctx.fillRect(rect.x,rect.y,rect.w,rect.h);
    const label=Math.max(9,Math.min(14,rect.h/10));ctx.font=`${label}px system-ui`;ctx.textAlign='center';ctx.textBaseline='middle';ctx.fillStyle='#e5edf7';
    if(state?.supported&&state.kind==='display'){
      let image=images.get(state);
      if(!image){image=document.createElement('canvas');image.width=state.width;image.height=state.height;const c=image.getContext('2d'),data=c.createImageData(state.width,state.height);data.data.set(rgba(state));c.putImageData(data,0,0);images.set(state,image);}
      const scale=Math.min(rect.w/image.width,(rect.h-label*2)/image.height),w=image.width*scale,h=image.height*scale;
      ctx.imageSmoothingEnabled=false;ctx.drawImage(image,rect.x+(rect.w-w)/2,rect.y+(rect.h-label*2-h)/2,w,h);
      ctx.fillText(summary(state),rect.x+rect.w/2,rect.y+rect.h-label,rect.w-8);
    }else if(state?.supported&&state.kind==='keyboard'&&rect.h>=100){
      const gap=3,w=(rect.w-9*gap)/8,h=(rect.h-label*3-5*gap)/4;
      for(const key of keyboard(state)){const x=rect.x+gap+(key.id%8)*(w+gap),y=rect.y+gap+Math.floor(key.id/8)*(h+gap);
        ctx.fillStyle=key.value==='1'?'#37b88a':key.value==='0'?'#26374c':'#a83b91';ctx.fillRect(x,y,w,h);ctx.fillStyle='#fff';ctx.fillText(key.label,x+w/2,y+h/2,w);}
      ctx.fillText(`Queue ${number(state.count)??'X'} · overflow ${state.overflow}`,rect.x+rect.w/2,rect.y+rect.h-label,rect.w-8);
    }else ctx.fillText(summary(state),rect.x+rect.w/2,rect.y+rect.h/2,rect.w-8);
    ctx.restore();
  }
  return {supports,number,validKeyboardState,keyboard,rgba,summary,draw};
}));
