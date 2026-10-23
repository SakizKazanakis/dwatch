#!/usr/bin/env python3
"""Offline watchface visual renderer. Source PNG preserved; eZIP validated only.

Uses no external modules, firmware, BLE, or network.
"""
import argparse, base64, hashlib, json, struct
from pathlib import Path

PNG_DIM=(485,520)
EZIP_SHA="e4d2412b8c2736fffe9b68f729d6cbbd1b1baefe4f68f2010c16bf60bfc03d9b"

def png_info(b):
    if len(b)<33 or b[:8]!=bytes.fromhex("89504e470d0a1a0a") or b[12:16]!=b"IHDR":
        raise ValueError("Expected original PNG")
    w,h,depth,color=struct.unpack_from(">IIBB",b,16)
    if (w,h,depth,color)!=(485,520,8,2):
        raise ValueError("Expected original Monolith 485x520 RGB PNG")
    return {"width":w,"height":h,"sha256":hashlib.sha256(b).hexdigest()}

def ezip_info(b):
    h,=struct.unpack_from("<I",b,0)
    dims=((h>>10)&2047,(h>>21)&2047,h&31)
    sha=hashlib.sha256(b).hexdigest()
    if len(b)!=158937 or dims!=(485,520,1) or sha!=EZIP_SHA:
        raise ValueError("Monolith eZIP exact-file check FAILED")
    return {"size":len(b),"dimensions":dims,"sha256":sha,"decoded":False}

TEMPLATE=r'''<!doctype html><html lang="ru"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Monolith — offline face renderer</title>
<style>body{background:#0c1013;color:#dfe8ed;font:14px system-ui;margin:22px auto;max-width:680px;padding:0 12px}
h1{letter-spacing:.22em;font-size:21px;font-weight:500}
aside,footer{color:#90a4b0;line-height:1.6}
section{display:flex;gap:14px;flex-wrap:wrap;align-items:center;margin:20px 0}
select,button{background:#293036;border:1px solid #57666e;color:#e7eef1;padding:10px;border-radius:6px}
canvas{display:block;max-width:100%;height:auto;margin:0 auto;box-shadow:0 15px 40px #0009}
pre{white-space:pre-wrap;overflow-wrap:anywhere;font-size:11px}</style>
</head><body><h1>MONOLITH</h1><aside>Независимый рендерер · оригинальная графика DKRNN · не прошивка часов</aside>
<section><label>Область просмотра <select id="mode">
<option value="source">485 × 520 · оригинал</option>
<option value="contain">410 × 494 · contain</option>
<option value="cover">410 × 494 · cover</option>
<option value="crop">410 × 494 · центр без масштаба</option></select></label>
<label><input id="time" type="checkbox" checked> Живое время</label>
<button id="save">Сохранить PNG</button></section>
<canvas id="canvas" width="485" height="520"></canvas>
<footer><p id="status"></p><details><summary>Источник и проверка</summary><pre id="meta"></pre></details>
<p>Встроен исходный PNG без изменений. eZIP проверяется по заголовку и SHA-256,
но его нестандартный поток здесь не декодируется. Никакой записи на HK8.</p></footer>
<script>
"use strict";
const info=__EVIDENCE__,original=new Image();original.src=__ORIGINAL__;
const cvs=document.getElementById("canvas"),ctx=cvs.getContext("2d");
const mode=document.getElementById("mode"),show=document.getElementById("time");
document.getElementById("meta").textContent=JSON.stringify(info,null,2);
function render(){
 if(!original.complete||!original.naturalWidth)return;
 const m=mode.value,w=m==="source"?485:410,h=m==="source"?520:494;
 if(cvs.width!==w||cvs.height!==h){cvs.width=w;cvs.height=h;}
 ctx.fillStyle="#11171b";ctx.fillRect(0,0,w,h);
 let dw=485,dh=520,dx=0,dy=0;
 if(m==="contain"||m==="cover"){
  const k=m==="contain"?Math.min(w/485,h/520):Math.max(w/485,h/520);
  dw=485*k;dh=520*k;dx=(w-dw)/2;dy=(h-dh)/2;
 }else if(m==="crop"){dx=(w-485)/2;dy=(h-520)/2;}
 ctx.drawImage(original,dx,dy,dw,dh);
 if(show.checked){
  const now=new Date(),scale=Math.min(w/485,h/520);
  const hh=now.toLocaleTimeString("ru-RU",{hour:"2-digit",minute:"2-digit",hour12:false});
  const dd=now.toLocaleDateString("ru-RU",{day:"2-digit",month:"short",year:"numeric"});
  ctx.save();ctx.textAlign="center";ctx.textBaseline="middle";
  ctx.fillStyle="#ecf2f4";ctx.shadowColor="#859ba7";ctx.shadowBlur=13*scale;
  ctx.font="300 "+Math.round(80*scale)+"px monospace";
  ctx.fillText(hh,w/2,h*.49);
  ctx.shadowBlur=0;ctx.fillStyle="#9fb4c1";
  ctx.font=Math.round(13*scale)+"px monospace";ctx.fillText(dd.toUpperCase(),w/2,h*.59);
  ctx.restore();
 }
 document.getElementById("status").textContent=m+" · "+w+"×"+h+" · original PNG SHA256: "+info.png.sha256;
}
original.onload=render;mode.onchange=render;show.onchange=render;setInterval(render,1000);
document.getElementById("save").onclick=function(){
 render();const link=document.createElement("a");
 link.download="monolith-offline-preview.png";link.href=cvs.toDataURL("image/png");link.click();
};
</script></body></html>'''

def build(png,ezip,output):
    source=Path(png).read_bytes()
    info={"png":png_info(source),"ezip":ezip_info(Path(ezip).read_bytes()) if ezip else None,
          "stock_oem_compatibility":"NOT VERIFIED"}
    content=TEMPLATE.replace("__EVIDENCE__",json.dumps(info)).replace(
      "__ORIGINAL__",json.dumps("data:image/png;base64,"+base64.b64encode(source).decode()))
    dst=Path(output);dst.parent.mkdir(parents=True,exist_ok=True)
    dst.write_text(content,encoding="utf-8")
    return {"path":str(dst),"size":dst.stat().st_size,"verified":info}

if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--original-png",required=True)
    p.add_argument("--original-ezip")
    p.add_argument("--output",default="monolith-offline-renderer.html")
    a=p.parse_args()
    print(json.dumps(build(a.original_png,a.original_ezip,a.output),indent=2))
