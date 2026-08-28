from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter, ImageOps

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'src' / 'pages-vitality-park' / 'static'
DESIGN = ROOT / 'design' / 'vitality-park'
for folder in ('backgrounds', 'props', 'character', 'events', 'effects'):
    (OUT / folder).mkdir(parents=True, exist_ok=True)
DESIGN.mkdir(parents=True, exist_ok=True)

# Reference sheets are kept for design review only. No labels or sample UI are
# used by runtime assets.
scene_ref = Image.open(r'C:\Users\jings\AppData\Local\Temp\codex-clipboard-a9b2dee7-1a95-41c2-9051-c18ad7ab8495.png').convert('RGBA')
character_ref = Image.open(r'C:\Users\jings\AppData\Local\Temp\codex-clipboard-f384176a-2742-4676-bdbf-7420a3014162.png').convert('RGBA')
scene_ref.save(DESIGN / 'reference-scenes.png')
character_ref.save(DESIGN / 'reference-character.png')


def alpha_cleanup(image, threshold=18):
    image = image.convert('RGBA')
    alpha = image.getchannel('A').point(lambda value: 0 if value <= threshold else value)
    image.putalpha(alpha)
    return image


def crop_alpha(image, box, padding=5):
    image = alpha_cleanup(image.crop(box))
    bbox = image.getchannel('A').getbbox()
    if not bbox:
        return Image.new('RGBA', (1, 1), (0, 0, 0, 0))
    left, top, right, bottom = bbox
    return image.crop((max(0, left - padding), max(0, top - padding), min(image.width, right + padding), min(image.height, bottom + padding)))


def save_asset(image, path, size=None):
    image = alpha_cleanup(image)
    if size:
        image.thumbnail(size, Image.Resampling.LANCZOS)
    image.save(path, optimize=True)


def extract_component(source, box):
    """Extract one alpha-connected object from a transparent sprite sheet."""
    x1, y1, x2, y2 = box
    crop = source.crop(box).convert('RGBA')
    alpha = crop.getchannel('A')
    # seed from the strongest pixel in this tight component box
    seed = max(((alpha.getpixel((x, y)), x, y) for y in range(crop.height) for x in range(crop.width)), key=lambda value: value[0])
    seen = set()
    stack = [(seed[1], seed[2])]
    while stack:
        x, y = stack.pop()
        if (x, y) in seen or not (0 <= x < crop.width and 0 <= y < crop.height) or alpha.getpixel((x, y)) < 48:
            continue
        seen.add((x, y))
        stack.extend(((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)))
    output = Image.new('RGBA', crop.size, (0, 0, 0, 0))
    src_pixels = crop.load()
    dst_pixels = output.load()
    for x, y in seen:
        dst_pixels[x, y] = src_pixels[x, y]
    return crop_alpha(output, (0, 0, output.width, output.height), padding=3)


# Use only the clean upper-left park plate. It includes the intended three
# depth bands: sky/buildings, path/fountain and foreground grass/flowers.
background = scene_ref.crop((32, 38, 410, 370)).convert('RGB')
background = ImageOps.fit(background, (1280, 720), method=Image.Resampling.LANCZOS)
background = background.filter(ImageFilter.UnsharpMask(radius=0.8, percent=115, threshold=3))
background.save(OUT / 'backgrounds' / 'park-background.webp', quality=92, method=6)

# Independent bench layer, cropped from the scene-object row. It is never
# included in a character pose.
bench = extract_component(scene_ref, (777, 70, 1090, 272))
bench.thumbnail((720, 430), Image.Resampling.LANCZOS)
bench.save(OUT / 'props' / 'bench.png', optimize=True)


# The new character board has six poses in equal 256px columns. The source
# alpha includes gray generation residue, so keep only body-shaped regions.
# These masks deliberately exclude the chair while retaining shirt, trousers,
# hands and shoes.
POSE_MASKS = {
    'sitting': [
        ('ellipse', (70, 105, 190, 255)), ('polygon', [(83, 235), (174, 225), (190, 350), (100, 360)]),
        ('polygon', [(88, 255), (116, 250), (146, 350), (120, 370), (96, 330)]), ('polygon', [(160, 245), (187, 250), (226, 345), (205, 367), (174, 330)]),
        ('polygon', [(108, 340), (210, 340), (244, 410), (205, 440), (120, 410)]),
        ('polygon', [(120, 390), (170, 394), (174, 535), (126, 545)]), ('polygon', [(172, 394), (224, 405), (218, 535), (170, 542)]),
        ('ellipse', (112, 515, 190, 585)), ('ellipse', (166, 512, 250, 582)),
    ],
    'lean-forward': [
        ('ellipse', (67, 118, 190, 268)), ('polygon', [(65, 245), (142, 198), (202, 242), (182, 345), (104, 374), (66, 320)]),
        ('polygon', [(80, 266), (112, 250), (151, 350), (125, 375), (94, 340)]), ('polygon', [(142, 235), (170, 230), (224, 338), (203, 365), (166, 325)]),
        ('polygon', [(105, 340), (210, 340), (247, 410), (210, 440), (118, 410)]),
        ('polygon', [(118, 390), (170, 393), (174, 535), (124, 545)]), ('polygon', [(170, 393), (225, 405), (218, 535), (169, 542)]),
        ('ellipse', (108, 515, 188, 585)), ('ellipse', (164, 512, 250, 582)),
    ],
    'lift-off': [
        ('ellipse', (67, 75, 194, 230)), ('polygon', [(67, 205), (140, 164), (202, 210), (183, 330), (104, 367), (65, 300)]),
        ('polygon', [(80, 230), (110, 215), (150, 335), (124, 365), (92, 320)]), ('polygon', [(145, 200), (173, 197), (220, 330), (198, 355), (164, 310)]),
        ('polygon', [(104, 325), (203, 318), (242, 405), (205, 435), (116, 405)]),
        ('polygon', [(116, 382), (168, 382), (173, 535), (122, 545)]), ('polygon', [(169, 382), (220, 396), (218, 535), (168, 542)]),
        ('ellipse', (106, 515, 188, 585)), ('ellipse', (162, 512, 248, 582)),
    ],
    'half-standing': [
        ('ellipse', (67, 37, 196, 192)), ('polygon', [(67, 168), (139, 130), (202, 175), (184, 310), (105, 342), (66, 270)]),
        ('polygon', [(79, 195), (108, 185), (143, 310), (118, 340), (89, 292)]), ('polygon', [(148, 170), (177, 170), (218, 305), (196, 330), (165, 282)]),
        ('polygon', [(104, 302), (197, 298), (235, 398), (199, 428), (116, 396)]),
        ('polygon', [(115, 366), (166, 364), (172, 535), (122, 545)]), ('polygon', [(166, 364), (215, 382), (217, 535), (166, 542)]),
        ('ellipse', (106, 515, 188, 585)), ('ellipse', (160, 512, 246, 582)),
    ],
    'standing': [
        ('ellipse', (65, 5, 198, 160)), ('polygon', [(77, 135), (181, 128), (190, 315), (88, 315)]),
        ('polygon', [(73, 160), (101, 160), (105, 340), (78, 360), (66, 280)]), ('polygon', [(168, 158), (197, 158), (207, 340), (181, 360), (172, 275)]),
        ('polygon', [(88, 295), (142, 295), (145, 525), (101, 545)]), ('polygon', [(140, 295), (190, 295), (204, 525), (158, 545)]),
        ('ellipse', (84, 510, 166, 585)), ('ellipse', (145, 510, 230, 582)),
    ],
    'sit-back': [
        ('ellipse', (67, 110, 192, 260)), ('polygon', [(80, 240), (175, 228), (190, 350), (98, 360)]),
        ('polygon', [(86, 260), (114, 255), (148, 350), (122, 372), (94, 330)]), ('polygon', [(160, 248), (188, 252), (226, 345), (204, 368), (174, 332)]),
        ('polygon', [(106, 340), (210, 340), (244, 412), (205, 442), (118, 410)]),
        ('polygon', [(118, 392), (170, 395), (174, 535), (124, 545)]), ('polygon', [(171, 395), (224, 406), (218, 535), (169, 542)]),
        ('ellipse', (108, 515, 190, 585)), ('ellipse', (164, 512, 250, 582)),
    ],
}
POSE_BOXES = {
    'sitting': (0, 25, 256, 650),
    'lean-forward': (278, 25, 536, 650),
    'lift-off': (566, 18, 828, 650),
    'half-standing': (784, 12, 1046, 650),
    'standing': (1024, 4, 1280, 650),
    'sit-back': (1280, 25, 1536, 650),
}

POSE_POLYGONS = {
    'sitting': [(86, 18), (174, 12), (224, 70), (226, 210), (255, 245), (255, 618), (112, 618), (102, 430), (72, 300), (70, 180)],
    'lean-forward': [(75, 18), (170, 12), (225, 80), (232, 214), (255, 270), (255, 618), (104, 618), (92, 430), (58, 282), (45, 150)],
    'lift-off': [(75, 15), (174, 10), (224, 80), (228, 220), (255, 288), (255, 618), (104, 618), (88, 410), (49, 270), (47, 136)],
    'half-standing': [(76, 10), (174, 8), (222, 88), (222, 260), (255, 320), (255, 618), (92, 618), (74, 340), (50, 224), (48, 110)],
    'standing': [(77, 8), (180, 8), (224, 90), (224, 618), (60, 618), (48, 240), (48, 94)],
    'sit-back': [(78, 18), (176, 12), (224, 72), (230, 215), (255, 250), (255, 618), (108, 618), (95, 420), (66, 285), (48, 150)],
}


def build_pose(name):
    source = character_ref.crop(POSE_BOXES[name]).copy()
    mask = Image.new('L', source.size, 0)
    ImageDraw.Draw(mask).polygon(POSE_POLYGONS[name], fill=255)
    # Preserve opaque illustrated edges but discard the low-alpha gray fringe.
    alpha = source.getchannel('A').point(lambda value: 0 if value < 42 else value)
    source.putalpha(Image.composite(alpha, Image.new('L', source.size, 0), mask))
    # The reference pose sheet still contains the chair behind the subject.
    # Remove its brown wood and blue-gray metal by color only in the lower
    # body region; hair, shirt and skin remain untouched.
    pixels = source.load()
    for y in range(400, source.height):
        for x in range(source.width):
            r, g, b, a = pixels[x, y]
            wood = r > 65 and r > g * 1.22 and g > b * 1.25 and b < 125
            metal = r < 100 and g > r * 1.18 and b > r * 1.18
            if a and (wood or metal):
                pixels[x, y] = (r, g, b, 0)
    # Remove only the outer chair-frame strips. The chair's remaining central
    # pixels are hidden behind the independent bench at runtime.
    for y in range(370, source.height):
        for x in range(0, 30):
            pixels[x, y] = (0, 0, 0, 0)
        for x in range(238, source.width):
            pixels[x, y] = (0, 0, 0, 0)
    if not source.getchannel('A').getbbox():
        return Image.new('RGBA', (520, 680), (0, 0, 0, 0))
    canvas = Image.new('RGBA', (520, 680), (0, 0, 0, 0))
    # Every pose comes from the same master scale. Preserve aspect ratio and
    # align the common ground line instead of stretching each crop to fit.
    scale = 1.04
    source = source.resize((round(source.width * scale), round(source.height * scale)), Image.Resampling.LANCZOS)
    canvas.alpha_composite(source, ((canvas.width - source.width) // 2, canvas.height - source.height - 12))
    return canvas


for pose_name in POSE_BOXES:
    build_pose(pose_name).save(OUT / 'character' / f'{pose_name}.png', optimize=True)


# Scene objects are cropped from the transparent component sheet. Coordinates
# avoid the captions and circular UI shown in the reference board.
props_ref = scene_ref
prop_boxes = {
    'bird': (303, 581, 406, 670),
    'butterfly': (462, 591, 553, 665),
    'flowers': (1195, 578, 1264, 684),
    'lamp': (1088, 18, 1162, 325),
    'fountain': (764, 270, 1114, 472),
    'kite': (898, 570, 978, 668),
    'dog': (650, 544, 815, 681),
}
for name, box in prop_boxes.items():
    save_asset(extract_component(props_ref, box), OUT / 'events' / f'{name}.png', (360, 260))

flags = Image.new('RGBA', (520, 150), (0, 0, 0, 0))
flag_draw = ImageDraw.Draw(flags)
flag_draw.line((8, 16, 512, 27), fill=(116, 76, 43, 210), width=4)
flag_colors = [(238, 79, 72, 240), (255, 191, 53, 240), (52, 158, 202, 240), (80, 177, 95, 240)]
for index in range(12):
    left = 18 + index * 41
    top = 16 + int(index * 11 / 12)
    flag_draw.polygon([(left, top), (left + 31, top + 1), (left + 15, top + 41)], fill=flag_colors[index % len(flag_colors)])
flags.save(OUT / 'events' / 'flags.png', optimize=True)


def make_rainbow(path):
    image = Image.new('RGBA', (520, 260), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    colors = [(239, 87, 91, 210), (255, 166, 44, 205), (255, 220, 66, 200), (95, 188, 108, 195), (71, 151, 224, 190)]
    for index, color in enumerate(colors):
        inset = 12 + index * 17
        draw.arc((inset, inset, image.width - inset, image.height * 2 - inset), 187, 353, fill=color, width=16)
    image.save(path, optimize=True)


def make_confetti(path, width=520, height=280):
    image = Image.new('RGBA', (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    colors = [(255, 205, 46, 230), (241, 92, 101, 220), (64, 170, 222, 220), (92, 190, 96, 220)]
    for index in range(48):
        x = (index * 83 + 31) % width
        y = (index * 47 + 17) % height
        draw.rounded_rectangle((x, y, x + 8, y + 16), radius=3, fill=colors[index % len(colors)])
    image.save(path, optimize=True)


make_rainbow(OUT / 'events' / 'rainbow.png')
make_confetti(OUT / 'events' / 'celebration.png')
make_confetti(OUT / 'effects' / 'celebration.png', 384, 240)

effect_burst = Image.new('RGBA', (256, 256), (0, 0, 0, 0))
ImageDraw.Draw(effect_burst).ellipse((22, 22, 234, 234), outline=(255, 218, 64, 205), width=11)
effect_burst.save(OUT / 'effects' / 'event-burst.png', optimize=True)
effect_stars = Image.new('RGBA', (256, 256), (0, 0, 0, 0))
draw = ImageDraw.Draw(effect_stars)
for index in range(10):
    x = 20 + (index * 47) % 216
    y = 24 + (index * 71) % 210
    draw.regular_polygon((x, y, 6 + index % 4), n_sides=5, rotation=18, fill=(255, 244, 128, 220))
effect_stars.save(OUT / 'effects' / 'star-particles.png', optimize=True)
