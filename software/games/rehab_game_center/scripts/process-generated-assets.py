from __future__ import annotations

from pathlib import Path
from collections import deque
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
REFERENCE = ROOT / "design" / "reference" / "generated"
STATIC = ROOT / "src" / "pages-fruit-game" / "static"


def contain(subject: Image.Image, size: tuple[int, int], padding: int = 12) -> Image.Image:
    canvas = Image.new("RGBA", size, (0, 0, 0, 0))
    bounds = subject.getbbox()
    if not bounds:
        return canvas
    cropped = subject.crop(bounds)
    usable = (size[0] - padding * 2, size[1] - padding * 2)
    scale = min(usable[0] / cropped.width, usable[1] / cropped.height)
    resized = cropped.resize(
        (max(1, round(cropped.width * scale)), max(1, round(cropped.height * scale))),
        Image.Resampling.LANCZOS,
    )
    x = (size[0] - resized.width) // 2
    y = (size[1] - resized.height) // 2
    canvas.alpha_composite(resized, (x, y))
    return canvas


def remove_connected_magenta(image: Image.Image) -> Image.Image:
    """Remove only border-connected chroma pixels so rainbow magenta stays intact."""
    rgba = image.convert("RGBA")
    pixels = rgba.load()
    width, height = rgba.size
    visited = bytearray(width * height)
    queue: deque[tuple[int, int]] = deque()

    def candidate(x: int, y: int) -> bool:
        red, green, blue, _ = pixels[x, y]
        return red > 140 and blue > 130 and green < 140 and abs(red - blue) < 130

    for x in range(width):
        if candidate(x, 0):
            queue.append((x, 0))
        if candidate(x, height - 1):
            queue.append((x, height - 1))
    for y in range(height):
        if candidate(0, y):
            queue.append((0, y))
        if candidate(width - 1, y):
            queue.append((width - 1, y))

    while queue:
        x, y = queue.popleft()
        index = y * width + x
        if visited[index] or not candidate(x, y):
            continue
        visited[index] = 1
        red, green, blue, _ = pixels[x, y]
        pixels[x, y] = (red, green, blue, 0)
        if x > 0:
            queue.append((x - 1, y))
        if x + 1 < width:
            queue.append((x + 1, y))
        if y > 0:
            queue.append((x, y - 1))
        if y + 1 < height:
            queue.append((x, y + 1))
    return rgba


def contract_magenta_fringe(image: Image.Image, passes: int = 2) -> Image.Image:
    output = image.copy()
    for _ in range(passes):
        source = output.copy()
        src = source.load()
        dst = output.load()
        for y in range(1, source.height - 1):
            for x in range(1, source.width - 1):
                red, green, blue, alpha = src[x, y]
                if alpha == 0 or not (red > 125 and blue > 120 and green < 145):
                    continue
                if any(src[x + dx, y + dy][3] == 0 for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1))):
                    dst[x, y] = (red, green, blue, 0)
    return output


def build_arms() -> None:
    sheet = Image.open(REFERENCE / "arms-sleeveless-sheet-alpha.png").convert("RGBA")
    cell_width = sheet.width // 3
    poses = ("low", "mid", "high")
    # Generated sheet shoulder roots, measured in each equal-width cell.
    roots = ((238, 220), (118, 245), (74, 392))
    scale = 0.34
    output_size = (320, 320)
    right_anchor = (62, 112)
    left_anchor = (output_size[0] - right_anchor[0], right_anchor[1])
    arm_dir = STATIC / "character"
    arm_dir.mkdir(parents=True, exist_ok=True)

    for index, pose in enumerate(poses):
        cell = sheet.crop((index * cell_width, 0, (index + 1) * cell_width, sheet.height))
        resized = cell.resize(
            (round(cell.width * scale), round(cell.height * scale)),
            Image.Resampling.LANCZOS,
        )
        root = (round(roots[index][0] * scale), round(roots[index][1] * scale))

        right = Image.new("RGBA", output_size, (0, 0, 0, 0))
        right.alpha_composite(resized, (right_anchor[0] - root[0], right_anchor[1] - root[1]))
        right.save(arm_dir / f"right-{pose}.png", optimize=True)

        mirrored = resized.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
        mirrored_root = resized.width - root[0]
        left = Image.new("RGBA", output_size, (0, 0, 0, 0))
        left.alpha_composite(mirrored, (left_anchor[0] - mirrored_root, left_anchor[1] - root[1]))
        left.save(arm_dir / f"left-{pose}.png", optimize=True)


def build_special_assets() -> None:
    sheet = remove_connected_magenta(
        Image.open(REFERENCE / "special-effects-sheet-magenta.png")
    )
    cell_width = sheet.width // 3
    cell_height = sheet.height // 2
    names = (
        ("fruits", "golden-apple.png"),
        ("fruits", "rainbow-fruit.png"),
        ("fruits", "both-watermelon.png"),
        ("effects", "combo.png"),
        ("effects", "reward-star.png"),
        ("effects", "harvest-burst.png"),
    )
    for index, (directory, filename) in enumerate(names):
        column = index % 3
        row = index // 3
        cell = sheet.crop(
            (
                column * cell_width,
                row * cell_height,
                (column + 1) * cell_width,
                (row + 1) * cell_height,
            )
        )
        size = (192, 192) if directory == "effects" else (256, 256)
        output = contract_magenta_fringe(contain(cell, size, 10))
        target_dir = STATIC / directory
        target_dir.mkdir(parents=True, exist_ok=True)
        output.save(target_dir / filename, optimize=True)


def build_backgrounds() -> None:
    backgrounds = STATIC / "backgrounds"
    backgrounds.mkdir(parents=True, exist_ok=True)
    for source_name, target_name in (
        ("orchard-sunset-master.png", "orchard-sunset.webp"),
        ("basket-zone-master.png", "basket-zone.webp"),
    ):
        image = Image.open(REFERENCE / source_name).convert("RGB")
        target_ratio = 16 / 9
        ratio = image.width / image.height
        if ratio > target_ratio:
            width = round(image.height * target_ratio)
            left = (image.width - width) // 2
            image = image.crop((left, 0, left + width, image.height))
        elif ratio < target_ratio:
            height = round(image.width / target_ratio)
            top = (image.height - height) // 2
            image = image.crop((0, top, image.width, top + height))
        image = image.resize((1280, 720), Image.Resampling.LANCZOS)
        image.save(backgrounds / target_name, "WEBP", quality=76, method=6)


if __name__ == "__main__":
    build_arms()
    build_special_assets()
    build_backgrounds()
    print("Generated runtime arms, special sprites, and backgrounds.")
