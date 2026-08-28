from PIL import Image, ImageFilter, ImageEnhance
import numpy as np, cv2
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'design/vitality-park/character-pose-master-v1.png'
OUT = ROOT / 'src/pages-vitality-park/static'
CHAR = OUT/'character'; BG=OUT/'backgrounds'; PROPS=OUT/'props'; EVENTS=OUT/'events'; EFFECTS=OUT/'effects'
for d in (CHAR,BG,PROPS,EVENTS,EFFECTS): d.mkdir(parents=True, exist_ok=True)

im = Image.open(SRC).convert('RGBA')
a = np.array(im.getchannel('A'))
mask=(a>20).astype(np.uint8)
num, labels, stats, cent = cv2.connectedComponentsWithStats(mask, 8)

def component(idx, pad=4):
    x,y,w,h,area=map(int,stats[idx])
    x0=max(0,x-pad); y0=max(0,y-pad); x1=min(im.width,x+w+pad); y1=min(im.height,y+h+pad)
    return im.crop((x0,y0,x1,y1))

def trim_alpha(img, threshold=3, pad=2):
    aa=np.array(img.getchannel('A'))
    ys,xs=np.where(aa>threshold)
    if len(xs)==0: return img
    l=max(0,int(xs.min())-pad); r=min(img.width,int(xs.max())+pad+1)
    t=max(0,int(ys.min())-pad); b=min(img.height,int(ys.max())+pad+1)
    return img.crop((l,t,r,b))

def bottom_anchor_x(img):
    aa=np.array(img.getchannel('A'))
    ys,xs=np.where(aa>32)
    bottom=int(ys.max())
    band=(aa[max(0,bottom-26):bottom+1,:]>32)
    by,bx=np.where(band)
    if len(bx)==0: return img.width//2
    # Right-facing character has both shoes; median gives stable foot centre.
    return int(np.median(bx))

def make_pose(idx, out_name):
    crop=trim_alpha(component(idx,3))
    # Standard transparent canvas. All poses share the same foot anchor and body scale.
    canvas=Image.new('RGBA',(260,430),(0,0,0,0))
    ax=bottom_anchor_x(crop)
    left=130-ax
    top=416-crop.height
    canvas.alpha_composite(crop,(left,top))
    canvas.save(CHAR/out_name)

# Source component IDs: consistent character mother sheet, no chairs.
for idx,name in [(4,'sitting.png'),(6,'lean-forward.png'),(3,'lift-off.png'),(2,'half-standing.png'),(1,'standing.png'),(5,'sit-back.png')]:
    make_pose(idx,name)

# Independent bench.
bench=trim_alpha(component(7,4))
bench.save(PROPS/'bench.png')

# Crisp far background from the text-free park layer. Upscale once at source-production time.
far=component(10,0)
far=far.resize((1280,720), Image.Resampling.LANCZOS)
base=Image.new('RGBA',far.size,(157,219,255,255))
base.alpha_composite(far)
base.convert('RGB').save(BG/'park-far.webp','WEBP',quality=94,method=6)

# Mid layer: natural shrubs at the sides; leave the centre open for fountain/event growth.
mid=Image.new('RGBA',(1280,720),(0,0,0,0))
for idx,pos,maxw in [(14,(0,350),360),(15,(915,342),365)]:
    c=trim_alpha(component(idx,2)); scale=min(1.75,maxw/c.width); c=c.resize((int(c.width*scale),int(c.height*scale)),Image.Resampling.LANCZOS); mid.alpha_composite(c,pos)
mid.save(BG/'park-mid.png')

# Foreground layer: low grass/flower clusters on screen edges only, never over the character.
front=Image.new('RGBA',(1280,720),(0,0,0,0))
for idx,pos,scale in [(24,(15,585),2.2),(31,(1020,610),2.0),(33,(0,650),1.8),(35,(1110,635),2.5)]:
    c=trim_alpha(component(idx,2)); c=c.resize((int(c.width*scale),int(c.height*scale)),Image.Resampling.LANCZOS); front.alpha_composite(c,pos)
front.save(BG/'park-foreground.png')

# Event assets integrated into the park (no circular icon containers).
# Bird perched / butterfly / dog / kite / flags / lamp / rainbow.
for idx,name in [(27,'bird.png'),(25,'butterfly.png'),(22,'dog.png'),(23,'kite.png'),(17,'flags.png'),(16,'lamp.png'),(40,'rainbow.png')]:
    trim_alpha(component(idx,3)).save(EVENTS/name)
# Fountain and flower bed are two areas of the same connected source component.
cf=component(11,2)
# crop relative areas; preserve alpha.
fountain=trim_alpha(cf.crop((0,0,cf.width,205)))
flowers=trim_alpha(cf.crop((0,178,cf.width,cf.height)))
fountain.save(EVENTS/'fountain.png'); flowers.save(EVENTS/'flowers.png')
# Celebration: clean confetti/event burst from the source sheet.
trim_alpha(component(58,3)).save(EVENTS/'celebration.png')

# Keep existing effect images; add a soft ground shadow generated locally.
shadow=Image.new('RGBA',(420,110),(0,0,0,0))
arr=np.zeros((110,420,4),dtype=np.uint8)
y,x=np.ogrid[:110,:420]
d=((x-210)/190)**2+((y-55)/33)**2
alpha=np.clip((1-d)*62,0,62).astype(np.uint8)
arr[:,:,3]=alpha
Image.fromarray(arr,'RGBA').filter(ImageFilter.GaussianBlur(9)).save(EFFECTS/'ground-shadow.png')

print('Vitality Park assets rebuilt from high-resolution transparent source sheet.')
