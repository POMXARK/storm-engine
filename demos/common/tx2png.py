"""Converts .tx textures to PNG to look at them: generated ones or those of a game.

Usage: tx2png.py <file.tx or folder> [output folder]
Each texture is written as <name>.png (the largest mip) next to it or into the output folder,
and its size, format and mip count are printed.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from storm_formats import read_tx, tx_info  # noqa: E402


def main():
    source = Path(sys.argv[1])
    output = Path(sys.argv[2]) if len(sys.argv) > 2 else None
    files = sorted(source.rglob("*.tx")) if source.is_dir() else [source]
    for path in files:
        target = (output / path.relative_to(source if source.is_dir() else path.parent)) if output else path
        target = target.with_name(target.name.removesuffix(".tx").removesuffix(".tga") + ".png")
        target.parent.mkdir(parents=True, exist_ok=True)
        try:
            read_tx(path).save(target)
            print(f"{path}: {tx_info(path)} -> {target}")
        except ValueError as error:
            print(f"{path}: {error}")


if __name__ == "__main__":
    main()
