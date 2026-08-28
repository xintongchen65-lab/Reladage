from __future__ import annotations

from pathlib import Path

from collections import deque

from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
REFERENCE = ROOT / "design" / "mole" / "reference"
STATIC = ROOT / "src" / "pages-mole-game" / "static"


def open_sheet(name: str) -> Image.Image:
    return Image.open(REFERENCE / name).convert("RGBA")


def crop_visible(sheet: Image.Image, box: tuple[int, int, int, int]) -> Image.Image:
    subject = sheet.crop(box)
    bounds = subject.getbbox()
    if not bounds:
        raise ValueError(f"Crop {box} has no visible pixels")
    return subject.crop(bounds)


def keep_largest_components(image: Image.Image, count: int = 1) -> Image.Image:
    rgba = image.copy()
    alpha = rgba.getchannel("A")
    pixels = alpha.load()
    width, height = alpha.size
    visited = bytearray(width * height)
    components: list[list[tuple[int, int]]] = []
    for start_y in range(height):
        for start_x in range(width):
            start_index = start_y * width + start_x
            if visited[start_index] or pixels[start_x, start_y] <= 18:
                continue
            queue: deque[tuple[int, int]] = deque(((start_x, start_y),))
            component: list[tuple[int, int]] = []
            while queue:
                x, y = queue.popleft()
                index = y * width + x
                if visited[index] or pixels[x, y] <= 18:
                    continue
                visited[index] = 1
                component.append((x, y))
                for next_x, next_y in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                    if 0 <= next_x < width and 0 <= next_y < height:
                        queue.append((next_x, next_y))
            if component:
                components.append(component)
    components.sort(key=len, reverse=True)
    keep = {y * width + x for component in components[:count] for x, y in component}
    output = rgba.copy()
    output_pixels = output.load()
    for y in range(height):
        for x in range(width):
            if y * width + x not in keep:
                red, green, blue, _ = output_pixels[x, y]
                output_pixels[x, y] = (red, green, blue, 0)
    return output


def contract_transparent_edge(image: Image.Image, passes: int = 2) -> Image.Image:
    output = image.copy()
    for _ in range(passes):
        source_alpha = output.getchannel("A")
        src = source_alpha.load()
        rgba = output.load()
        clear: list[tuple[int, int]] = []
        for y in range(1, output.height - 1):
            for x in range(1, output.width - 1):
                if src[x, y] <= 0:
                    continue
                if any(src[x + dx, y + dy] <= 18 for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1))):
                    clear.append((x, y))
        for x, y in clear:
            red, green, blue, _ = rgba[x, y]
            rgba[x, y] = (red, green, blue, 0)
    return output


def apply_shape_mask(
    image: Image.Image,
    polygons: tuple[tuple[tuple[int, int], ...], ...],
    ellipses: tuple[tuple[int, int, int, int], ...] = (),
) -> Image.Image:
    mask = Image.new("L", image.size, 0)
    drawing = ImageDraw.Draw(mask)
    for polygon in polygons:
        drawing.polygon(polygon, fill=255)
    for ellipse in ellipses:
        drawing.ellipse(ellipse, fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(1.2))
    original_alpha = image.getchannel("A")
    image = image.copy()
    image.putalpha(Image.composite(original_alpha, Image.new("L", image.size, 0), mask))
    return image


def contain(
    subject: Image.Image,
    canvas_size: tuple[int, int],
    subject_box: tuple[int, int],
    *,
    bottom_padding: int = 10,
    max_upscale: float = 1.35,
) -> Image.Image:
    bounds = subject.getbbox()
    if not bounds:
        raise ValueError("Subject has no visible pixels")
    subject = subject.crop(bounds)
    scale = min(
        subject_box[0] / subject.width,
        subject_box[1] / subject.height,
        max_upscale,
    )
    size = (max(1, round(subject.width * scale)), max(1, round(subject.height * scale)))
    if size != subject.size:
        subject = subject.resize(size, Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", canvas_size, (0, 0, 0, 0))
    x = (canvas.width - subject.width) // 2
    y = canvas.height - bottom_padding - subject.height
    canvas.alpha_composite(subject, (x, y))
    return canvas


def save_png(image: Image.Image, relative: str) -> None:
    target = STATIC / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    image.save(target, "PNG", optimize=True)


def build_background(sheet: Image.Image) -> None:
    # The supplied master contains a complete 16:9 five-hole scene. Flatten only
    # its rounded transparent edge pixels; no resampling or artificial upscaling.
    scene = sheet.crop((8, 1, 830, 464))
    flattened = Image.new("RGB", scene.size, (104, 201, 239))
    flattened.paste(scene, mask=scene.getchannel("A"))
    target = STATIC / "backgrounds" / "meadow.webp"
    target.parent.mkdir(parents=True, exist_ok=True)
    flattened.save(target, "WEBP", quality=92, method=6)


def build_moles(sheet: Image.Image) -> None:
    # Prominent supplied characters. Each crop stops above the baked dirt ring;
    # the runtime's fixed hole layer covers the intentionally softened lower edge.
    specifications = (
        (
            (838, 4, 1063, 169),
            (((49, 0), (174, 0), (196, 52), (184, 96), (211, 111), (188, 124), (159, 132), (73, 132), (43, 123), (18, 109), (43, 96), (36, 48)),),
            (),
        ),
        (
            (1066, 4, 1277, 174),
            (
                ((58, 70), (26, 51), (4, 69), (2, 109), (29, 123), (63, 132), (82, 105)),
                ((151, 70), (181, 51), (207, 70), (210, 109), (182, 123), (149, 132), (131, 105)),
                ((56, 7), (157, 7), (176, 34), (181, 75), (174, 108), (157, 126), (55, 126), (35, 108), (28, 75), (35, 34)),
            ),
            (),
        ),
        (
            (1280, 4, 1535, 176),
            (
                ((77, 67), (42, 47), (8, 68), (6, 111), (44, 130), (80, 134), (97, 103)),
                ((177, 68), (211, 48), (247, 68), (251, 111), (213, 130), (177, 134), (157, 103)),
                ((74, 7), (181, 7), (201, 35), (205, 75), (198, 108), (180, 127), (73, 127), (49, 107), (44, 75), (49, 35)),
            ),
            (),
        ),
        (
            (1062, 183, 1278, 369),
            (
                ((60, 70), (28, 48), (4, 67), (2, 112), (35, 136), (69, 137), (84, 108)),
                ((156, 70), (188, 48), (213, 67), (214, 112), (181, 136), (148, 137), (132, 108)),
                ((55, 4), (163, 4), (186, 32), (192, 76), (184, 112), (164, 133), (50, 133), (29, 111), (24, 76), (32, 32)),
            ),
            (),
        ),
        (
            (836, 181, 1068, 369),
            (
                ((72, 75), (46, 58), (12, 70), (6, 112), (31, 136), (71, 137), (91, 107)),
                ((62, 4), (178, 4), (197, 35), (201, 78), (194, 111), (176, 132), (55, 132), (35, 110), (30, 78), (37, 35)),
            ),
            (),
        ),
    )
    for index, (box, polygons, ellipses) in enumerate(specifications, start=1):
        crop = sheet.crop(box)
        crop = apply_shape_mask(crop, polygons, ellipses)
        crop = contract_transparent_edge(crop, 2)
        sprite = contain(crop, (340, 260), (316, 224), bottom_padding=12)
        save_png(sprite, f"moles/mole-{index}.png")


def build_objects(primary: Image.Image, composition: Image.Image) -> None:
    hole = contract_transparent_edge(keep_largest_components(crop_visible(composition, (22, 538, 286, 654))), 1)
    save_png(contain(hole, (300, 132), (280, 112), bottom_padding=8, max_upscale=1.05), "objects/hole.png")

    hammer = contract_transparent_edge(keep_largest_components(crop_visible(primary, (4, 462, 191, 672))), 2)
    save_png(contain(hammer, (230, 270), (208, 250), bottom_padding=8, max_upscale=1.12), "objects/hammer.png")

    sign = contract_transparent_edge(keep_largest_components(crop_visible(primary, (5, 674, 309, 818))), 1)
    save_png(contain(sign, (330, 158), (316, 144), bottom_padding=6, max_upscale=1.04), "objects/sign.png")

    carrot = contract_transparent_edge(keep_largest_components(crop_visible(primary, (84, 777, 151, 873))), 1)
    save_png(contain(carrot, (128, 160), (104, 136), bottom_padding=10, max_upscale=1.12), "objects/carrot.png")

    coin = contract_transparent_edge(keep_largest_components(crop_visible(primary, (2, 775, 88, 869))), 1)
    save_png(contain(coin, (144, 144), (120, 120), bottom_padding=10, max_upscale=1.12), "objects/coin.png")

    star = contract_transparent_edge(keep_largest_components(crop_visible(primary, (150, 772, 252, 874))), 1)
    save_png(contain(star, (152, 152), (128, 128), bottom_padding=10, max_upscale=1.12), "objects/star.png")


def build_effects(primary: Image.Image) -> None:
    burst = contract_transparent_edge(keep_largest_components(crop_visible(primary, (1016, 509, 1145, 638))), 1)
    save_png(contain(burst, (220, 220), (198, 198), bottom_padding=11, max_upscale=1.25), "effects/hit-burst.png")

    warning = contract_transparent_edge(keep_largest_components(crop_visible(primary, (1314, 505, 1536, 680)), 5), 1)
    save_png(contain(warning, (300, 238), (276, 214), bottom_padding=12, max_upscale=1.12), "effects/warning-ring.png")


def main() -> None:
    primary = open_sheet("supplied-primary-sheet.png")
    composition = open_sheet("supplied-composition-sheet.png")
    build_background(primary)
    build_moles(primary)
    build_objects(primary, composition)
    build_effects(primary)
    print("Built mole runtime assets directly from the four supplied sprite sheets.")


if __name__ == "__main__":
    main()
