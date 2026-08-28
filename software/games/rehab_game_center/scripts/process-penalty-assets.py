"""Crop generated masters into compact, transparent penalty-game runtime assets."""
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
DESIGN = ROOT / "design" / "penalty"
OUT = ROOT / "src" / "pages-penalty-game" / "static"


def transparent_magenta(image: Image.Image) -> Image.Image:
    image = image.convert("RGBA")
    pixels = image.load()
    for y in range(image.height):
        for x in range(image.width):
            r, g, b, a = pixels[x, y]
            if x < 18 or x >= image.width - 18 or y < 24 or y >= image.height - 24:
                pixels[x, y] = (r, g, b, 0)
                continue
            # Generated key backgrounds vary slightly around pure magenta.
            if r > 150 and b > 110 and g < 175 and r > g * 1.15 and b > g * 1.05:
                pixels[x, y] = (r, g, b, 0)
            elif r > 185 and b > 135 and g < 135 and r > g * 1.45:
                pixels[x, y] = (r, g, b, min(a, 70))
    return image


def tight_crop(image: Image.Image, padding: int = 16) -> Image.Image:
    alpha = image.getchannel("A")
    box = alpha.getbbox()
    if not box:
        return image
    left, top, right, bottom = box
    return image.crop((max(0, left-padding), max(0, top-padding), min(image.width, right+padding), min(image.height, bottom+padding)))


def save_fit(image: Image.Image, path: Path, size: tuple[int, int]) -> None:
    image = tight_crop(transparent_magenta(image))
    image.thumbnail(size, Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", size, (0, 0, 0, 0))
    x = (size[0] - image.width) // 2
    y = size[1] - image.height
    canvas.alpha_composite(image, (x, y))
    path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(path, optimize=True)


def crop_sheet(source: Path, count: int, names: list[tuple[str, tuple[int, int]]]) -> None:
    sheet = Image.open(source)
    cell = sheet.width / count
    for index, (relative, size) in enumerate(names):
        left = round(index * cell)
        right = round((index + 1) * cell)
        # Shave a few pixels so white separator lines never enter runtime sprites.
        part = sheet.crop((left + 5, 0, right - 5, sheet.height))
        save_fit(part, OUT / relative, size)


def main() -> None:
    background = Image.open(DESIGN / "stadium-master.png").convert("RGB")
    background.thumbnail((1280, 720), Image.Resampling.LANCZOS)
    (OUT / "backgrounds").mkdir(parents=True, exist_ok=True)
    background.save(OUT / "backgrounds" / "stadium.webp", "WEBP", quality=80, method=6)
    crop_sheet(DESIGN / "player-sheet-master.png", 5, [
        ("player/neutral.png", (300, 500)), ("player/left-flex.png", (300, 500)),
        ("player/left-kick.png", (300, 500)), ("player/right-flex.png", (300, 500)),
        ("player/right-kick.png", (300, 500)),
    ])
    crop_sheet(DESIGN / "keeper-effects-sheet-master.png", 8, [
        ("keeper/ready.png", (250, 300)), ("keeper/dive-left.png", (300, 250)),
        ("keeper/dive-right.png", (300, 250)), ("objects/ball.png", (150, 150)),
        ("effects/goal.png", (260, 260)), ("effects/save.png", (260, 260)),
        ("effects/miss.png", (260, 260)), ("effects/combo.png", (260, 260)),
    ])


if __name__ == "__main__":
    main()
