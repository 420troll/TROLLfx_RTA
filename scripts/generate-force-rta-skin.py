#!/usr/bin/env python3
"""Generate source-controlled native MPC skin art and parameter-bound LED ladders."""
import json
import math
import os
from PIL import Image, ImageDraw, ImageFont

GRAPH_STEPS = 129
GRAPH_FRAME_COUNT = GRAPH_STEPS * GRAPH_STEPS + 1
GRAPH_MISSING_FRAME = GRAPH_FRAME_COUNT - 1

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

def response_panel(filename, mode):
    im=Image.new("RGBA",(1280,628),(8,4,5,255)); d=ImageDraw.Draw(im)
    d.rectangle((0,0,1279,47),fill=(18,5,7,255));d.line((0,47,1279,47),fill=(115,24,31,255),width=2)
    d.text((22,9),"TROLLfx / RTA  ·  V0.5.4  ·  BUILD 504",font=font(22,True),fill=(255,75,76,255))
    d.text((708,14),mode,font=font(17,True),fill=(220,108,104,255))
    if mode == "SWEEP LED RESULTS":
        x0,x1,y0,y1=70,1008,58,450
        d.rectangle((x0,y0,x1,y1),fill=(10,5,6,255),outline=(91,22,27,255),width=2)
        for q in range(5):
            y=y0+q*(y1-y0)/4
            d.line((x0,y,x1,y),fill=(111,31,36,255) if q==2 else (56,25,29,255),width=2 if q==2 else 1)
        centers=["20","25","31","40","50","63","80","100","125","160","200","250","315","400","500","630","800","1k","1k2","1k6","2k","2k5","3k1","4k","5k","6k3","8k","10k","12k","16k","20k"]
        gap=(x1-x0)/31; width=19; start=x0+(gap-width)/2
        for i,label in enumerate(centers):
            x=start+i*gap+width/2
            d.text((x,457),label,font=font(11,True),fill=(239,154,151,255),anchor="mt")
            if i%2==0:d.line((x,450,x,454),fill=(130,51,55,255),width=1)
        d.text((75,487),"LED LEVEL WINDOW · COMMON SCALE FOR ALL BANDS · RELATIVE TO SMOOTHED 1 kHz",font=font(13,True),fill=(199,89,91,255))
    else:
        x0,x1,y0,y1=74,1008,54,443
        d.rectangle((x0,y0,x1,y1),fill=(10,5,6,255),outline=(91,22,27,255),width=2)
        def xfreq(hz): return x0+(math.log(hz/20.0)/math.log(1000.0))*(x1-x0)
        for q in range(5):
            y=y0+q*(y1-y0)/4
            db=12-q*12
            d.line((x0,y,x1,y),fill=(128,43,49,255) if db==0 else (64,27,31,255),width=2 if db==0 else 1)
        for hz,label in [(20,"20"),(31.5,"31.5"),(50,"50"),(100,"100"),(200,"200"),(500,"500"),(1000,"1k"),(2000,"2k"),(5000,"5k"),(10000,"10k"),(20000,"20k")]:
            x=xfreq(hz);d.line((x,y0,x,y1),fill=(52,25,28,255),width=1)
            d.text((x,449),label,font=font(12,True),fill=(248,173,169,255),anchor="mt")
    d.rectangle((18,511,1008,619),fill=(16,7,8,255),outline=(81,25,29,255),width=1)
    labels=[("SMOOTHING",122),("MEASUREMENT",123),("RANGE",250),("CENTER",251)]
    for slot,(label,_) in enumerate(labels):
        x=26+slot*245
        d.text((x,515),label,font=font(12,True),fill=(210,107,106,255))
    d.text((26,588),"WINDOW dB re 1 kHz:",font=font(11,True),fill=(196,99,99,255))
    d.text((219,588),"LOW",font=font(10,True),fill=(176,82,84,255))
    d.text((396,588),"HIGH",font=font(10,True),fill=(176,82,84,255))
    d.text((590,588),"Selected saved sweep · display only",font=font(11),fill=(151,96,96,255))
    im.save(os.path.join(ROOT,filename))

def graph_segment_strip():
    """SDK Image filmstrip prototype: frame index packs adjacent y samples."""
    width,height=4,64
    strip=Image.new("RGBA",(width,height*GRAPH_FRAME_COUNT),(0,0,0,0))
    draw=ImageDraw.Draw(strip)
    for top in range(GRAPH_STEPS):
        y0=round(top*(height-1)/(GRAPH_STEPS-1))
        for bottom in range(GRAPH_STEPS):
            y1=round(bottom*(height-1)/(GRAPH_STEPS-1))
            frame=top*GRAPH_STEPS+bottom
            base=frame*height
            draw.polygon([(0,base+y0),(width-1,base+y1),(width-1,base+height-1),(0,base+height-1)],fill=(255,35,46,32))
            draw.line([(0,base+y0),(width-1,base+y1)],fill=(255,46,55,255),width=1)
    # Last frame (missing data) remains transparent.
    strip.save(os.path.join(ROOT,"sweep-graph-segments.png"),optimize=True)

def clipping_strip():
    w,h=24,392; im=Image.new("RGBA",(w,h*4),(0,0,0,0));d=ImageDraw.Draw(im)
    d.polygon([(1,h+16),(12,h+2),(23,h+16),(19,h+16),(12,h+8),(5,h+16)],fill=(255,142,133,255))
    d.polygon([(1,2*h+h-17),(12,2*h+h-2),(23,2*h+h-17),(19,2*h+h-17),(12,2*h+h-9),(5,2*h+h-17)],fill=(255,87,78,255))
    d.line((3,3*h+h//2,20,3*h+h//2),fill=(255,100,76,220),width=3)
    d.line((3,3*h+h//2-7,20,3*h+h//2+7),fill=(255,100,76,220),width=2)
    im.save(os.path.join(ROOT,"sweep-clipping.png"))

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

def tab(name, idx, entries, sub=0):
    return {"version":1,"tabName":name,"fnKeyIndex":idx,"fnKeySubIndex":sub,"componentDefinition":root(entries),"qlinkBoundsData":[]}

def draw_touch_button(stem, title, width, height):
    """Generate matching states for the SDK's native, parameter-bound Button."""
    for active in (False, True):
        im=Image.new("RGBA",(width,height),(25,10,14,255))
        d=ImageDraw.Draw(im)
        fill=(119,23,35,255) if active else (38,18,24,255)
        outline=(255,110,118,255) if active else (137,56,66,255)
        d.rounded_rectangle((2,2,width-3,height-3),radius=9,fill=fill,outline=outline,width=3)
        d.text((width//2,height//2),title,font=font(22,True),anchor="mm",fill=(255,246,242,255))
        im.save(os.path.join(ROOT,stem+("_on.png" if active else "_off.png")))

def touch_button(stem,index,x,y,width,height,button_id=1,group_size=1):
    data={"version":2,"onImage":stem+"_on.png","offImage":stem+"_off.png",
          "buttonId":button_id,"numButtonsInGroup":group_size,
          "handleName":"Data","gestureBehaviour":"Instant"}
    return entry(stem,"Button",data,f"{x} {y} {width} {height}",index,"Yes")

def main():
    ladder("led-ladder.png"); ladder("peak-ladder.png",True)
    draw_touch_button("sweep-start-touch","START SWEEP",350,84)
    draw_touch_button("sweep-cancel-touch","CANCEL / FADE",350,84)
    for slot,label in enumerate(["LATEST","PREVIOUS 2","PREVIOUS 3","PREVIOUS 4",
                                 "PREVIOUS 5","PREVIOUS 6","PREVIOUS 7","PREVIOUS 8"]):
        draw_touch_button("sweep-slot-"+str(slot),label,230,76)
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
        stem="sweep-start-touch" if idx==115 else "sweep-cancel-touch"
        sweep_entries.append(touch_button(stem,idx,x,92,350,84))
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
    response_panel("sweep-led-panel.png","SWEEP LED RESULTS")
    clipping_strip(); graph_segment_strip()
    led_result_entries=[entry("sweepLedBackground","Image",{"version":2,"imageType":"Regular","colour":"0","image":"sweep-led-panel.png"},"0 0 1280 628")]
    gap=(1008-70)/31;width=19;start=70+(gap-width)/2
    for i in range(31):
        x=int(start+i*gap)
        led_result_entries.append(image_entry("sweepLedBand"+str(i),124+i,"led-ladder.png",x,y=58,w=width,h=392))
        led_result_entries.append(image_entry("sweepLedPeak"+str(i),218+i,"peak-ladder.png",x,y=58,w=width,h=392))
        led_result_entries.append(entry("sweepLedClip"+str(i),"Image",{"version":2,"imageType":"FilmStrip","colour":"0","image":"sweep-clipping.png","numFrames":4,"handleName":"Data"},f"{x-2} 58 {width+4} 392",509+i,"No"))
    for i in range(5):
        y=58+i*(450-58)/4
        led_result_entries.append(label_entry("ledDbTick"+str(i),254+i,14,y-9,50,18,size=11,color="ffff9996",align="right verticallyCentred"))
    led_result_entries.append(label_entry("ledResultStatus",217,745,12,250,20,size=14,color="ffff9b55"))
    led_result_entries.append(label_entry("ledSweepDiagnostic",249,745,31,250,14,size=10,color="ffff7777"))
    # The output values state the active range endpoints; changing the range
    # or center updates these without changing stored measurement values.
    led_result_entries.append(entry("ledLowLabel","Label",{"version":1,"textStyle":text_style(11,"ffff7777"),"type":"Value","handleName":"Data"},"243 587 90 22",252,"No"))
    led_result_entries.append(entry("ledHighLabel","Label",{"version":1,"textStyle":text_style(11,"ffff7777"),"type":"Value","handleName":"Data"},"420 587 90 22",253,"No"))
    controls=[(122,"SMOOTHING"),(123,"MEASUREMENT"),(250,"RANGE"),(251,"CENTER")]
    for slot,(idx,label) in enumerate(controls):
        x=26+slot*245
        led_result_entries.append(entry("ledResultLabel"+str(idx),"Label",{"version":1,"textStyle":text_style(12,"ffff7777",bold=True),"type":"Text","handleName":"Invalid","text":label},f"{x} 515 230 18"))
        led_result_entries.append(entry("ledResultControl"+str(idx),"Slider",{"version":1,"handleName":"Data"},f"{x} 534 230 46",idx,"Yes"))
    tabs.append(tab("SWEEP LED RESULTS",5,led_result_entries))

    response_panel("sweep-graph-panel.png","SWEEP GRAPH")
    graph_entries=[entry("sweepGraphBackground","Image",{"version":2,"imageType":"Regular","colour":"0","image":"sweep-graph-panel.png"},"0 0 1280 628")]
    graph_x0,graph_x1,graph_y0,graph_y1=74,1008,54,443
    segment_gap=(graph_x1-graph_x0)/255.0
    # A connected 256-sample polyline. Each documented Image film-strip
    # component selects a pre-rendered segment frame encoding adjacent y bins.
    # Frames share a 129x129 endpoint lookup and alpha-filled area image.
    for i in range(255):
        bounds=f"{graph_x0+i*segment_gap:.4f} {graph_y0} {segment_gap+0.04:.4f} {graph_y1-graph_y0}"
        data={"version":2,"imageType":"FilmStrip","colour":"0","image":"sweep-graph-segments.png","numFrames":GRAPH_FRAME_COUNT,"handleName":"Data"}
        graph_entries.append(entry(f"graphLineSegment{i}","Image",data,bounds,259+i,"No"))
    for i in range(31):
        hz=20.0*1000.0**(i/30.0);x=graph_x0+math.log(hz/20.0)/math.log(1000.0)*(graph_x1-graph_x0)
        graph_entries.append(entry("graphClip"+str(i),"Image",{"version":2,"imageType":"FilmStrip","colour":"0","image":"sweep-clipping.png","numFrames":4,"handleName":"Data"},f"{x-9:.2f} {graph_y0} 18 {graph_y1-graph_y0}",509+i,"No"))
    for i in range(5):
        y=graph_y0+i*(graph_y1-graph_y0)/4
        graph_entries.append(label_entry("graphDbTick"+str(i),254+i,14,y-9,52,18,size=12,color="ffff9996",align="right verticallyCentred"))
    graph_entries.append(label_entry("graphResultStatus",217,745,12,250,20,size=14,color="ffff9b55"))
    graph_entries.append(label_entry("graphSweepDiagnostic",249,745,31,250,14,size=10,color="ffff7777"))
    graph_entries.append(entry("graphLowLabel","Label",{"version":1,"textStyle":text_style(11,"ffff7777"),"type":"Value","handleName":"Data"},"243 587 90 22",252,"No"))
    graph_entries.append(entry("graphHighLabel","Label",{"version":1,"textStyle":text_style(11,"ffff7777"),"type":"Value","handleName":"Data"},"420 587 90 22",253,"No"))
    for slot,(idx,label) in enumerate([(122,"SMOOTHING"),(123,"MEASUREMENT"),(250,"RANGE"),(251,"CENTER")]):
        x=26+slot*245
        graph_entries.append(entry("graphResultLabel"+str(idx),"Label",{"version":1,"textStyle":text_style(12,"ffff7777",bold=True),"type":"Text","handleName":"Invalid","text":label},f"{x} 515 230 18"))
        graph_entries.append(entry("graphResultControl"+str(idx),"Slider",{"version":1,"handleName":"Data"},f"{x} 534 230 46",idx,"Yes"))
    tabs.append(tab("SWEEP GRAPH",5,graph_entries,1))
    # Saved-measurement selection is an eight-position choice (parameter 123).
    # SDK Button radio group uses zero-based IDs and does not alter session data.
    slot_entries=[entry("slotsBackground","Image",
                        {"version":2,"imageType":"Regular","colour":"0","image":"sweep-led-panel.png"},
                        "0 0 1280 628")]
    slot_entries.append(entry("slotsTitle","Label",
                              {"version":1,"textStyle":text_style(26,"ffffaaaa",bold=True),
                               "type":"Text","handleName":"Invalid",
                               "text":"SELECT SAVED SWEEP · NO RECORDING OR OVERWRITE"},
                              "80 72 1120 52"))
    for slot in range(8):
        x=96+(slot%4)*270
        y=176+(slot//4)*155
        slot_entries.append(touch_button("sweep-slot-"+str(slot),123,x,y,230,76,slot,8))
    slot_entries.append(label_entry("slotsStatus",217,825,532,270,38,size=20,color="ffff9b55"))
    tabs.append(tab("SAVED SWEEPS",5,slot_entries,2))
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
    for t, tab_data in enumerate(tabs):
        assignments={f"Q-Link {n}":-1 for n in range(1,17)}
        if t==2:
            for n in range(1,11): assignments[f"Q-Link {n}"]=100+n
            assignments["Q-Link 11"]=111; assignments["Q-Link 12"]=112; assignments["Q-Link 13"]=113
        if tab_data["tabName"]=="SWEEP":
            for n,idx in enumerate((118,119,120,121),1): assignments[f"Q-Link {n}"]=idx
            assignments["Q-Link 5"]=122; assignments["Q-Link 6"]=123
        if tab_data["tabName"] == "SAVED SWEEPS":
            assignments["Q-Link 6"]=123
        if tab_data["tabName"] in ("SWEEP LED RESULTS","SWEEP GRAPH"):
            assignments["Q-Link 5"]=122; assignments["Q-Link 6"]=123
            assignments["Q-Link 7"]=250; assignments["Q-Link 8"]=251
        qlinks.append({"Tab":tab_data["fnKeyIndex"]+1,"SubTab":tab_data["fnKeySubIndex"]+1,"Bank Direction":"Column","Q-Links":assignments})
    q={"version":4,"info":{"version":1,"type":"CompleteDescription"},"Screen Mode Q-Links":{"version":4,"map":qlinks},"Program Mode Q-Links":{f"Q-Link {n}":-1 for n in range(1,17)}}
    with open(os.path.join(ROOT,"Q-Links.json"),"w") as f: json.dump(q,f,indent=2)
    print("Generated MPC TUI/Q-Link skins, red LED assets and pair-coded 256-point sweep graph filmstrip in",ROOT)

if __name__=="__main__": main()
