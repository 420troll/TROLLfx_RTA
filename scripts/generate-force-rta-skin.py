#!/usr/bin/env python3
"""Generate source-controlled native MPC skin art and parameter-bound LED ladders."""
import json
import math
import os
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "plugins", "TrollfxRta", "skins"))
os.makedirs(ROOT, exist_ok=True)
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
FONT_BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"

def font(size, bold=False):
    try: return ImageFont.truetype(FONT_BOLD if bold else FONT, size)
    except OSError: return ImageFont.load_default()

def ladder(name, peak=False):
    w, h, steps = 28, 420, 25
    im = Image.new("RGBA", (w, h*steps), (0,0,0,0))
    d = ImageDraw.Draw(im)
    for frame in range(steps):
        top = frame*h
        if peak:
            if frame > 0:
                level = max(0, min(24, frame))
                y = top + h - 9 - level*(h-12)//24
                d.rounded_rectangle((3,y,25,y+5), radius=2, fill=(255,176,142,255))
        else:
            level = frame
            for seg in range(24):
                y0 = top + h - 10 - (seg+1)*(h-14)//24
                y1 = top + h - 10 - seg*(h-14)//24 - 2
                color = (229,27,37,255) if seg < level else (53,14,18,255)
                d.rounded_rectangle((5,y0,23,y1), radius=2, fill=color)
    im.save(os.path.join(ROOT, name))

def panel(filename, centers, title):
    im=Image.new("RGBA",(1280,628),(8,4,5,255)); d=ImageDraw.Draw(im)
    d.rectangle((0,0,1279,61), fill=(18,5,7,255)); d.line((0,61,1279,61),fill=(115,24,31,255),width=2)
    d.text((24,13),"TROLLfx  /  RTA",font=font(26,True),fill=(255,75,76,255))
    d.text((975,21),title,font=font(17,True),fill=(196,88,85,255))
    d.rectangle((15,73,1265,542),fill=(10,5,6,255),outline=(91,22,27,255),width=2)
    d.line((20,520,1260,520),fill=(82,22,26,255),width=1)
    n=len(centers); gap=1240/n; width=max(16,int(gap*.56)); x0=20+(gap-width)/2
    for i,label in enumerate(centers):
        x=int(x0+i*gap+width/2)
        d.text((x,550),label,font=font(12),fill=(200,130,130,255),anchor="mt")
        if i%2==0:
            d.line((x,532,x,540),fill=(130,51,55,255),width=1)
    d.text((24,595),"CORRECTED BAND LEVEL  ·  DISPLAY GAIN / FLOOR APPLY TO LEDS ONLY",font=font(15),fill=(132,63,66,255))
    im.save(os.path.join(ROOT,filename))

def response_panel(filename):
    im=Image.new("RGBA",(1280,628),(8,4,5,255)); d=ImageDraw.Draw(im)
    d.rectangle((0,0,1279,61),fill=(18,5,7,255));d.line((0,61,1279,61),fill=(115,24,31,255),width=2)
    d.text((24,13),"TROLLfx / RTA  ·  V0.5.2  ·  BUILD 502",font=font(24,True),fill=(255,75,76,255))
    d.text((860,21),"SWEEP RESULTS  /  SAVED RESPONSE",font=font(16,True),fill=(196,88,85,255))
    x0,x1,y0,y1=82,1260,77,300
    d.rectangle((x0,y0,x1,y1),fill=(10,5,6,255),outline=(91,22,27,255),width=2)
    def xfreq(hz): return x0+(math.log(hz/20.0)/math.log(1000.0))*(x1-x0)
    for db in (6,0,-6,-12,-18,-24,-30):
        y=y0+(6-db)/36.0*(y1-y0)
        d.line((x0,y,x1,y),fill=(104,38,43,255) if db==0 else (57,24,28,255),width=2 if db==0 else 1)
        d.text((74,y),f"{db:+d}",font=font(12,True),fill=(201,126,126,255),anchor="rm")
    d.text((20,76),"dB",font=font(12,True),fill=(201,126,126,255))
    for hz,label in [(20,"20"),(31.5,"31"),(50,"50"),(100,"100"),(200,"200"),(500,"500"),(1000,"1k"),(2000,"2k"),(5000,"5k"),(10000,"10k"),(20000,"20k")]:
        x=xfreq(hz);d.line((x,y0,x,y1),fill=(47,22,25,255),width=1)
        d.text((x,306),label,font=font(12,True),fill=(218,150,150,255),anchor="mt")
    d.text((24,340),"31-BAND RESPONSE  ·  -30 TO +6 dB REL. 1 kHz",font=font(15,True),fill=(202,91,91,255))
    d.line((570,348,598,348),fill=(133,59,63,255),width=2);d.text((604,340),"RAW",font=font(12,True),fill=(177,111,111,255))
    d.line((660,348,690,348),fill=(255,38,51,255),width=3);d.text((697,340),"SMOOTHED",font=font(12,True),fill=(220,128,128,255))
    d.rectangle((15,363,1265,476),fill=(10,5,6,255),outline=(91,22,27,255),width=1)
    d.line((20,475,1260,475),fill=(82,22,26,255),width=1)
    centers=["20","25","31","40","50","63","80","100","125","160","200","250","315","400","500","630","800","1k","1k2","1k6","2k","2k5","3k1","4k","5k","6k3","8k","10k","12k","16k","20k"]
    gap=1240/len(centers);width=22;start=20+(gap-width)/2
    for i,label in enumerate(centers):
        x=int(start+i*gap+width/2);d.text((x,481),label,font=font(10,True),fill=(200,130,130,255),anchor="mt")
    im.save(os.path.join(ROOT,filename))

def entry(name, typ, data, bounds, handle=None, focus="No"):
    remap=[] if handle is None else [{"key":"Data","value":f"Parameter {handle}"}]
    return {"version":2,"componentData":{"version":1,"name":name,"type":typ,"data":data},
            "handle remapping":{"version":1,"map":remap},
            "bounds":{"version":1,"acceptsHWFocus":focus,"showWhenDataModelInvalid":"Hide" if handle is not None else "Show",
                       "whenVisible":"Always","boundsType":"Absolute","bounds":bounds}}

def image_entry(name, idx, asset, x, y=82, w=28, h=420):
    data={"version":2,"imageType":"FilmStrip","colour":"0","image":asset,"numFrames":25,"handleName":"Data"}
    return entry(name,"Image",data,f"{x} {y} {w} {h}",idx)

def plotter_entry(name, idx, colour, line_width, bounds):
    data={"version":2,"backgroundColour":"0","drawLine":True,"lineDirection":"Right","tightClip":True,
          "lineColourStart":colour,"lineColourEnd":colour,"horizLineGradient":False,"lineWidth":line_width,
          "fillColourStart":"0","fillColourEnd":"0","horizFillGradient":False,"fillZero":-24.0,
          "dataHandle":"Data","dataMin":-30.0,"dataMax":6.0,"drawHistory":False}
    data["fillZero"]=-30.0
    return entry(name,"Plotter",data,bounds,idx)

def text_style(size, color, align="left verticallyCentred", bold=False):
    return {"version":1,"font":{"version":1,"name":"Titillium Web","style":"Bold" if bold else "Regular","height":float(size)},
            "colour":color,"justification":align,"case":"Original"}

def label_entry(name, idx, x, y, w, h, typ="Value", size=30, color="ffff7777", align="left verticallyCentred"):
    data={"version":1,"textStyle":text_style(size,color,align),"type":typ,"handleName":"Data"}
    return entry(name,"Label",data,f"{x} {y} {w} {h}",idx)

def root(entries):
    return {"version":2,"actions":[],"backgroundData":{"version":1,"focussed":{"version":1,"colour":"ff080405","image":""},"unfocussed":{"version":1,"colour":"ff080405","image":""}},
            "ignoreMousePresses":False,"disableCoarseDataWheel":False,"componentsData":entries}

def tab(name, idx, entries):
    return {"version":1,"tabName":name,"fnKeyIndex":idx,"fnKeySubIndex":0,"componentDefinition":root(entries),"qlinkBoundsData":[]}

def main():
    ladder("led-ladder.png"); ladder("peak-ladder.png",True)
    centers31=["20","25","31","40","50","63","80","100","125","160","200","250","315","400","500","630","800","1k","1k2","1k6","2k","2k5","3k1","4k","5k","6k3","8k","10k","12k","16k","20k"]
    centers16=["20","31","50","80","125","200","315","500","800","1k2","2k","3k1","5k","8k","12k","20k"]
    panel("panel31.png",centers31,"31 BAND / 1-3 OCT")
    panel("panel16.png",centers16,"16 BAND / POWER SUM")
    tabs=[]
    for page,title,panelname,bandbase,peakbase,count in [(0,"31 BAND","panel31.png",0,31,31),(1,"16 BAND","panel16.png",68,84,16)]:
        es=[entry("background","Image",{"version":2,"imageType":"Regular","colour":"0","image":panelname},"0 0 1280 628")]
        gap=1240/count; width=max(16,int(gap*.56)); start=20+(gap-width)/2
        for i in range(count):
            x=int(start+i*gap)
            es.append(image_entry(f"band{i+1}",bandbase+i,"led-ladder.png",x,w=width))
            es.append(image_entry(f"peak{i+1}",peakbase+i,"peak-ladder.png",x,w=width))
        tabs.append(tab(title,page,es))
    control_entries=[]
    control_ids=list(range(101,111))
    labels=["DISPLAY GAIN","DISPLAY FLOOR","SMOOTHING","ANALYSIS AVG","HOLD ENABLE","RESET PEAKS","LED ATTACK","LED FALL","PEAK HOLD","PEAK FALL"]
    for slot,(idx,label) in enumerate(zip(control_ids,labels)):
        row,col=divmod(slot,4); x=23+col*320; y=60+row*150
        control_entries.append(entry("panelLabel"+str(slot),"Label",{"version":1,"textStyle":text_style(20,"ffff7777"),"type":"Text","handleName":"Invalid","text":label},f"{x} {y} 278 34"))
        control_entries.append(entry("control"+str(slot),"Slider",{"version":1,"handleName":"Data"},f"{x} {y+36} 278 78",idx,"Yes"))
    for slot,(idx,label) in enumerate([(111,"SERVICE MODE"),(112,"ANALYZER START"),(113,"ANALYZER STOP"),(114,"SERVICE STATUS")]):
        x=23+slot*320; y=510
        control_entries.append(entry("serviceLabel"+str(slot),"Label",{"version":1,"textStyle":text_style(18,"ffff7777"),"type":"Text","handleName":"Invalid","text":label},f"{x} {y} 278 28"))
        if idx==114: control_entries.append(label_entry("serviceStatus",idx,x,y+30,278,58,size=22,color="ffff9b55"))
        else: control_entries.append(entry("serviceControl"+str(slot),"Slider",{"version":1,"handleName":"Data"},f"{x} {y+30} 278 58",idx,"Yes"))
    tabs.append(tab("DISPLAY",2,control_entries))
    sweep_entries=[]
    for idx,label,x in [(115,"GENERATE / START SWEEP",55),(116,"CANCEL / FADE OUT",455)]:
        sweep_entries.append(entry("sweepLabel"+str(idx),"Label",{"version":1,"textStyle":text_style(22,"ffff7777"),"type":"Text","handleName":"Invalid","text":label},f"{x} 54 350 34"))
        sweep_entries.append(entry("sweepButton"+str(idx),"Slider",{"version":1,"handleName":"Data"},f"{x} 92 350 84",idx,"Yes"))
    sweep_entries.append(entry("sweepStatusLabel","Label",{"version":1,"textStyle":text_style(18,"ffff7777"),"type":"Text","handleName":"Invalid","text":"MEASUREMENT STATUS"},"865 54 350 34"))
    sweep_entries.append(label_entry("sweepStatus",117,865,92,350,84,size=26,color="ffff9b55"))
    for slot,(idx,label) in enumerate([(118,"DURATION (s)"),(119,"START FREQUENCY (Hz)"),(120,"STOP FREQUENCY (Hz)"),(121,"SAFE PEAK LEVEL (dBFS)")]):
        col=slot%2;row=slot//2;x=55+col*600;y=205+row*125
        sweep_entries.append(entry("sweepParamLabel"+str(slot),"Label",{"version":1,"textStyle":text_style(20,"ffff7777"),"type":"Text","handleName":"Invalid","text":label},f"{x} {y} 520 32"))
        sweep_entries.append(entry("sweepParam"+str(slot),"Slider",{"version":1,"handleName":"Data"},f"{x} {y+38} 520 78",idx,"Yes"))
    for idx,label,x in [(122,"RESPONSE SMOOTHING",55),(123,"SAVED MEASUREMENT",655)]:
        sweep_entries.append(entry("sweepOutputLabel"+str(idx),"Label",{"version":1,"textStyle":text_style(19,"ffff7777"),"type":"Text","handleName":"Invalid","text":label},f"{x} 468 550 30"))
        sweep_entries.append(entry("sweepOutputControl"+str(idx),"Slider",{"version":1,"handleName":"Data"},f"{x} 501 550 54",idx,"Yes"))
    sweep_entries.append(entry("sweepNote","Label",{"version":1,"textStyle":text_style(15,"ffbb7777"),"type":"Text","handleName":"Invalid","text":"Changes re-render the selected saved session; they do not replay or record a sweep. Results remain under Service/sessions."},"55 574 1160 34"))
    tabs.append(tab("SWEEP",4,sweep_entries))
    response_centers=["20","25","31","40","50","63","80","100","125","160","200","250","315","400","500","630","800","1k","1k2","1k6","2k","2k5","3k1","4k","5k","6k3","8k","10k","12k","16k","20k"]
    response_panel("response-panel.png")
    response_entries=[entry("responseBackground","Image",{"version":2,"imageType":"Regular","colour":"0","image":"response-panel.png"},"0 0 1280 628")]
    centers_hz=[20,25,31.5,40,50,63,80,100,125,160,200,250,315,400,500,630,800,1000,1250,1600,2000,2500,3150,4000,5000,6300,8000,10000,12500,16000,20000]
    graph_x0,graph_x1,graph_y0,graph_y1=82,1260,77,300
    def freq_x(hz): return graph_x0+math.log(hz/20.0)/math.log(1000.0)*(graph_x1-graph_x0)
    for i,hz in enumerate(centers_hz):
        left=graph_x0 if i==0 else freq_x(math.sqrt(centers_hz[i-1]*hz))
        right=graph_x1 if i==30 else freq_x(math.sqrt(hz*centers_hz[i+1]))
        bounds=f"{left:.2f} {graph_y0} {right-left:.2f} {graph_y1-graph_y0}"
        response_entries.append(plotter_entry(f"rawTrace{i}",186+i,"ff853b3f",1.1,bounds))
        response_entries.append(plotter_entry(f"smoothTrace{i}",155+i,"ffff2633",2.4,bounds))
    gap=1240/31;width=22;start=20+(gap-width)/2
    for i in range(31):response_entries.append(image_entry("responseBand"+str(i),124+i,"led-ladder.png",int(start+i*gap),y=365,w=width,h=108))
    response_entries.append(label_entry("resultStatus",217,875,336,360,28,size=18,color="ffff9b55"))
    for idx,label,x in [(122,"SMOOTHING",55),(123,"SAVED MEASUREMENT",655)]:
        response_entries.append(entry("resultControlLabel"+str(idx),"Label",{"version":1,"textStyle":text_style(15,"ffff7777"),"type":"Text","handleName":"Invalid","text":label},f"{x} 516 520 24"))
        response_entries.append(entry("resultControl"+str(idx),"Slider",{"version":1,"handleName":"Data"},f"{x} 541 520 56",idx,"Yes"))
    tabs.append(tab("SWEEP RESULTS",5,response_entries))
    level_entries=[]
    bg=Image.new("RGBA",(1280,628),(8,4,5,255)); d=ImageDraw.Draw(bg)
    d.rectangle((0,0,1279,61),fill=(18,5,7,255));d.line((0,61,1279,61),fill=(115,24,31,255),width=2)
    d.text((24,13),"TROLLfx  /  RTA  ·  MICROPHONE",font=font(26,True),fill=(255,75,76,255));d.text((24,112),"RAW INPUT LEVEL",font=font(19,True),fill=(174,80,80,255));d.text((24,265),"WEIGHTED SOUND PRESSURE LEVEL",font=font(19,True),fill=(174,80,80,255));d.text((690,305),"UMIK SENS FACTOR  ·  METADATA ONLY",font=font(16,True),fill=(174,80,80,255))
    bg.save(os.path.join(ROOT,"level-panel.png"))
    level_entries.append(entry("levelBackground","Image",{"version":2,"imageType":"Regular","colour":"0","image":"level-panel.png"},"0 0 1280 628"))
    level_entries.extend([label_entry("raw",62,24,145,600,76,size=58,color="ffff7777"),label_entry("lz",63,24,305,450,80,size=48),label_entry("la",64,24,400,450,80,size=48),label_entry("orientation",65,690,145,520,64,size=30),label_entry("mic",66,690,240,520,64,size=30),label_entry("calibration",67,24,525,850,64,size=32,color="ffff9b55"),label_entry("sensfactor",68,690,330,520,64,size=30)])
    tabs.append(tab("LEVEL",3,level_entries))
    tui={"pageData":{"version":1,"componentDefinitions":{"version":2,"importFiles":[],"localComponentDefinitions":[]},"replacementStrings":[],"info":{"version":1,"type":"CompleteDescription"},"tabs":tabs}}
    with open(os.path.join(ROOT,"TUI.json"),"w") as f: json.dump(tui,f,indent=2)
    qlinks=[]
    for t in range(len(tabs)):
        assignments={f"Q-Link {n}":-1 for n in range(1,17)}
        if t==2:
            for n in range(1,11): assignments[f"Q-Link {n}"]=100+n
            assignments["Q-Link 11"]=111; assignments["Q-Link 12"]=112; assignments["Q-Link 13"]=113
        if t==4:
            for n,idx in enumerate((118,119,120,121),1): assignments[f"Q-Link {n}"]=idx
            assignments["Q-Link 5"]=122; assignments["Q-Link 6"]=123
        if t==5:
            assignments["Q-Link 5"]=122; assignments["Q-Link 6"]=123
        qlinks.append({"Tab":t+1,"SubTab":1,"Bank Direction":"Column","Q-Links":assignments})
    q={"version":4,"info":{"version":1,"type":"CompleteDescription"},"Screen Mode Q-Links":{"version":4,"map":qlinks},"Program Mode Q-Links":{f"Q-Link {n}":-1 for n in range(1,17)}}
    with open(os.path.join(ROOT,"Q-Links.json"),"w") as f: json.dump(q,f,indent=2)
    print("Generated MPC TUI/Q-Link skins and 25-frame red LED assets in",ROOT)

if __name__=="__main__": main()
