#!/usr/bin/env python3
"""Pillow layout preview of the V0.5.3 skin; not an MPC firmware renderer."""
import math
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[1]
SKIN=ROOT/'plugins/TrollfxRta/skins'
OUT=ROOT/'docs/images'
FONT='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
BOLD='/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
def font(size,bold=False):
    try:return ImageFont.truetype(BOLD if bold else FONT,size)
    except OSError:return ImageFont.load_default()

def frame_asset(asset,frame,w,h):
    src=Image.open(SKIN/asset).convert('RGBA'); fh=src.height//25
    return src.crop((0,frame*fh,src.width,(frame+1)*fh)).resize((w,h),Image.Resampling.LANCZOS)

def host_frame(content,title):
    screen=Image.new('RGBA',(1280,800),(5,5,6,255));d=ImageDraw.Draw(screen)
    d.rectangle((0,0,1279,109),fill=(21,21,23,255));d.text((23,15),'TROLLfx RTA  ·  V0.5.3 / BUILD 503',font=font(24,True),fill=(250,105,107,255))
    d.text((24,58),title,font=font(18,True),fill=(245,220,220,255))
    d.text((740,64),'DISPLAY     SWEEP     LED RESULTS     GRAPH',font=font(15,True),fill=(179,179,182,255))
    screen.alpha_composite(content,(0,110))
    # Approximation of the documented Force Q-Link sidebar overlay. Its exact
    # firmware appearance and opacity are not represented by this preview.
    d=ImageDraw.Draw(screen,'RGBA');d.rectangle((1025,110,1279,799),fill=(24,24,26,237),outline=(85,85,87,255),width=2)
    d.text((1053,132),'Q-LINKS',font=font(18,True),fill=(235,235,235,255))
    for i in range(8):
        y=180+i*70;d.rounded_rectangle((1047,y,1258,y+48),radius=7,fill=(47,47,50,255),outline=(95,95,99,255))
        d.text((1060,y+14),f'Q{i+1}   SWEEP',font=font(14,True),fill=(212,205,205,255))
    d.text((1047,755),'SIMULATED OVERLAY',font=font(12,True),fill=(205,118,119,255))
    return screen.convert('RGB')

OUT.mkdir(parents=True,exist_ok=True)
# LED page response shows a broad low-frequency rise and a smaller treble rise.
led=Image.open(SKIN/'sweep-led-panel.png').convert('RGBA')
centers=[20*1000**(i/30) for i in range(31)]; gap=974/31;width=22;start=30+(gap-width)/2
for i,hz in enumerate(centers):
    db=-10-8*math.exp(-((math.log(hz/85))/0.7)**2)+3*math.exp(-((math.log(hz/7000))/0.5)**2)
    frame=max(0,min(24,round((db+30)*24/36)))
    x=round(start+i*gap);led.alpha_composite(frame_asset('led-ladder.png',frame,width,402),(x,63))
    led.alpha_composite(frame_asset('peak-ladder.png',frame,width,402),(x,63))
ld=ImageDraw.Draw(led);ld.text((760,512),'READY',font=font(15,True),fill=(255,174,90,255));ld.text((760,529),'OK',font=font(11,True),fill=(255,130,130,255))
ld.rounded_rectangle((30,574,480,616),radius=4,fill=(70,18,23,255),outline=(200,55,65,255));ld.text((42,586),'1/6 OCTAVE',font=font(15,True),fill=(255,190,190,255))
ld.rounded_rectangle((512,574,962,616),radius=4,fill=(70,18,23,255),outline=(200,55,65,255));ld.text((524,586),'LATEST MEASUREMENT',font=font(15,True),fill=(255,190,190,255))
host_frame(led,'SWEEP LED RESULTS — static selected saved measurement').save(OUT/'v053-sweep-led-layout-simulation.png')
# Filled, logarithmic response graph from illustrative 1/6-octave data.
graph=Image.open(SKIN/'sweep-graph-panel.png').convert('RGBA');dr=ImageDraw.Draw(graph,'RGBA')
x0,x1,y0,y1=84,1005,69,479;bar=min(27,(x1-x0)/30-2)
for i,hz in enumerate(centers):
    db=-10-8*math.exp(-((math.log(hz/85))/0.7)**2)+3*math.exp(-((math.log(hz/7000))/0.5)**2)
    db=max(-30,min(6,db)); cx=x0+math.log(hz/20)/math.log(1000)*(x1-x0); top=y0+(6-db)/36*(y1-y0)
    dr.rectangle((cx-bar/2,top,cx+bar/2,y1),fill=(255,32,45,170))
    dr.line((cx-bar/2,top,cx+bar/2,top),fill=(255,130,132,255),width=4)
dr=ImageDraw.Draw(graph);dr.text((760,512),'READY',font=font(15,True),fill=(255,174,90,255));dr.text((760,529),'OK',font=font(11,True),fill=(255,130,130,255))
dr.rounded_rectangle((30,574,480,616),radius=4,fill=(70,18,23,255),outline=(200,55,65,255));dr.text((42,586),'1/6 OCTAVE',font=font(15,True),fill=(255,190,190,255))
dr.rounded_rectangle((512,574,962,616),radius=4,fill=(70,18,23,255),outline=(200,55,65,255));dr.text((524,586),'LATEST MEASUREMENT',font=font(15,True),fill=(255,190,190,255))
host_frame(graph,'SWEEP GRAPH — 31-point parameter-bound sample preview').save(OUT/'v053-sweep-graph-layout-simulation.png')
print('Wrote Pillow layout simulations; not rendered by MPC firmware.')
