#!/usr/bin/env python3
"""Pillow mockup of the V0.5.4 skin, not a capture from MPC/Force firmware."""
import math
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[1]
SKIN=ROOT/'plugins/TrollfxRta/skins'
OUT=ROOT/'docs/images';OUT.mkdir(parents=True,exist_ok=True)
FONT='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf';BOLD='/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
def f(size,bold=False):
    try:return ImageFont.truetype(BOLD if bold else FONT,size)
    except OSError:return ImageFont.load_default()
def response_db(hz):
    x=math.log2(hz)
    return (4*math.exp(-.5*((x-math.log2(110))/0.14)**2)
            -17*math.exp(-.5*((x-math.log2(72))/0.095)**2)
            +5*math.exp(-.5*((x-math.log2(3200))/0.21)**2)
            -4*math.exp(-.5*((x-math.log2(11500))/0.16)**2))
def draw_controls(image, draw):
    draw.text((32,536),'1/6 octave',font=f(15,True),fill=(255,167,158,255))
    draw.text((275,536),'Latest sweep',font=f(15,True),fill=(255,167,158,255))
    draw.text((522,536),'48 dB',font=f(15,True),fill=(255,167,158,255))
    draw.text((767,536),'-12 dB',font=f(15,True),fill=(255,167,158,255))
    draw.text((259,588),'-36.0',font=f(12,True),fill=(255,193,185,255))
    draw.text((437,588),'+12.0',font=f(12,True),fill=(255,193,185,255))
def led_sim():
    im=Image.open(SKIN/'sweep-led-panel.png').convert('RGBA');d=ImageDraw.Draw(im)
    x0,x1,y0,y1=70,1008,58,450; minimum,maximum=-36,12
    for i in range(5):
        y=y0+i*(y1-y0)/4;db=maximum-i*(maximum-minimum)/4
        d.text((62,y-8),f'{db:+.0f}',font=f(10,True),fill=(255,152,144,255),anchor='rm')
    for hz in [20,25,31.5,40,50,63,80,100,125,160,200,250,315,400,500,630,800,1000,1250,1600,2000,2500,3150,4000,5000,6300,8000,10000,12500,16000,20000]:
        db=response_db(hz); level=max(0,min(24,round(24*(db-minimum)/(maximum-minimum))))
        gap=(x1-x0)/31;w=19;x=x0+(hz and (math.log(hz/20)/math.log(1000))*(x1-x0))-w/2
        for seg in range(24):
            yy1=y1-2-seg*(y1-y0)/24;yy0=y1-2-(seg+1)*(y1-y0)/24+2
            c=(233,27,39,255) if seg<level else (54,14,18,255)
            d.rounded_rectangle((round(x+2),round(yy0),round(x+w-2),round(yy1)),2,fill=c)
    draw_controls(im,d)
    im.save(OUT/'v054-sweep-led-layout-simulation.png')
def graph_sim():
    im=Image.open(SKIN/'sweep-graph-panel.png').convert('RGBA');d=ImageDraw.Draw(im)
    x0,x1,y0,y1=74,1008,54,443;mn,mx=-36,12
    strip=Image.open(SKIN/'sweep-graph-segments.png').convert('RGBA')
    scale=3;overlay=Image.new('RGBA',(1280*scale,628*scale),(0,0,0,0))
    vals=[]
    for i in range(256):
        hz=20*1000**(i/255);vals.append(response_db(hz))
    gap=(x1-x0)/255
    for i in range(5):
        y=y0+i*(y1-y0)/4;db=mx-i*(mx-mn)/4
        d.text((65,y),f'{db:+.0f}',font=f(11,True),fill=(255,157,150,255),anchor='rm')
    for i in range(255):
        a,b=vals[i],vals[i+1]
        qa=max(0,min(128,round((mx-a)*128/(mx-mn))))
        qb=max(0,min(128,round((mx-b)*128/(mx-mn))))
        frame=qa*129+qb
        left=round((x0+i*gap)*scale);right=round((x0+(i+1)*gap)*scale)
        sprite=strip.crop((0,frame*64,4,frame*64+64)).resize((right-left,(y1-y0)*scale),Image.Resampling.BILINEAR)
        overlay.alpha_composite(sprite,(left,y0*scale))
    overlay=overlay.resize((1280,628),Image.Resampling.LANCZOS)
    im.alpha_composite(overlay)
    draw_controls(im,d)
    im.save(OUT/'v054-sweep-graph-layout-simulation.png')
led_sim();graph_sim()
print('Wrote Pillow layout simulations; these are not MPC or Akai Force firmware captures.')
