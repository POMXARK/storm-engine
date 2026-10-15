#!/usr/bin/env python3
"""Converts glTF 2.0 models (.glb or .gltf, as Blender, Kenney or Sketchfab give them) to the
engine's static models: a .gm per file and a .tx per texture.

    gltf2gm.py <model.glb|.gltf|folder>... --out <resource dir> [--folder <name>] [--scale S]
               [--no-bsp] [--single-sided]

Every file becomes resource/models/<folder>/<name>.gm, loaded by MSG_MODEL_LOAD_GEO "<folder>\\<name>"
(or dropped onto Viewport from the Assets window of the editor). Its textures go to
resource/textures/<folder>/<image>.tga.tx, the name the .gm refers them by.

- The node tree of the scene is baked: every mesh primitive is one object of the .gm, in the place
  its nodes give it. Skins, morph targets and animations are not read (static models only).
- glTF is right-handed, the engine (D3D) left-handed, both with y up: x is mirrored, so the model
  looks the same and faces the same way (+z) as in Blender's glTF export.
- The base colour texture of a material is its texture; a material without one gets a texture of
  its base colour. A base colour factor tints the texture. Other maps (normals, metal) are dropped.
- doubleSided materials get their faces twice, once each way: the engine culls back faces.
- With the BSP tree (default) traces hit the model: picking in the editor, cameras, shots.
"""
import argparse
import base64
import json
import math
import struct
import sys
from io import BytesIO
from pathlib import Path

from PIL import Image

# the .tx/.gm writers are shared with the demo generators; the build copies them next to this file
# (build/<config>/tools/gltf2gm, run by the button Import of the editor)
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE if (HERE / "storm_formats.py").exists() else HERE.parents[1] / "demos" / "common"))
from storm_formats import Mesh, write_gm, write_tx  # noqa: E402

COMPONENTS = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}
FORMATS = {5120: "b", 5121: "B", 5122: "h", 5123: "H", 5125: "I", 5126: "f"}
# a normalized integer accessor is divided by the largest value of its type
NORMALIZE = {5120: 127.0, 5121: 255.0, 5122: 32767.0, 5123: 65535.0}
MODE_TRIANGLES = 4


def warn(text):
    print(f"gltf2gm: {text}", file=sys.stderr)


# ---------------------------------------------------------------- reading

class Gltf:
    def __init__(self, path):
        self.path = Path(path)
        data = self.path.read_bytes()
        self.bin = None
        if data[:4] == b"glTF":
            # header, then chunks: JSON first, BIN (the first buffer) next
            offset = 12
            while offset < len(data):
                length, kind = struct.unpack_from("<I4s", data, offset)
                chunk = data[offset + 8:offset + 8 + length]
                if kind == b"JSON":
                    self.json = json.loads(chunk)
                elif kind == b"BIN\0":
                    self.bin = chunk
                offset += 8 + length
        else:
            self.json = json.loads(data)
        self.buffers = [self.load(buffer.get("uri"), i) for i, buffer in enumerate(self.json.get("buffers", []))]

    def load(self, uri, index=0):
        if uri is None:
            if self.bin is None:
                raise ValueError(f"{self.path}: buffer {index} has no data")
            return self.bin
        if uri.startswith("data:"):
            return base64.b64decode(uri.split(",", 1)[1])
        return (self.path.parent / uri.replace("%20", " ")).read_bytes()

    def view(self, index):
        view = self.json["bufferViews"][index]
        start = view.get("byteOffset", 0)
        return self.buffers[view["buffer"]], start, view.get("byteStride")

    def accessor(self, index):
        """The elements of an accessor as tuples of floats (or ints for indices)."""
        acc = self.json["accessors"][index]
        count, size = acc["count"], COMPONENTS[acc["type"]]
        fmt = FORMATS[acc["componentType"]]
        if "bufferView" not in acc:
            values = [tuple([0] * size)] * count
        else:
            buffer, start, stride = self.view(acc["bufferView"])
            start += acc.get("byteOffset", 0)
            item = struct.calcsize("<" + fmt * size)
            stride = stride or item
            values = [struct.unpack_from("<" + fmt * size, buffer, start + i * stride) for i in range(count)]
        if acc.get("normalized") and acc["componentType"] in NORMALIZE:
            scale = NORMALIZE[acc["componentType"]]
            values = [tuple(max(c / scale, -1.0) for c in v) for v in values]
        if "sparse" in acc:
            warn(f"{self.path.name}: sparse accessor {index} read without its sparse values")
        return values

    def image(self, index):
        image = self.json["images"][index]
        if "bufferView" in image:
            buffer, start, _ = self.view(image["bufferView"])
            length = self.json["bufferViews"][image["bufferView"]]["byteLength"]
            data = buffer[start:start + length]
        else:
            data = self.load(image["uri"])
        return Image.open(BytesIO(data)).convert("RGBA")


# ---------------------------------------------------------------- matrices (row-major 4x4 lists)

def multiply(a, b):
    return [[sum(a[r][k] * b[k][c] for k in range(4)) for c in range(4)] for r in range(4)]


def node_matrix(node):
    if "matrix" in node:
        m = node["matrix"]  # column-major
        return [[m[c * 4 + r] for c in range(4)] for r in range(4)]
    tx, ty, tz = node.get("translation", (0.0, 0.0, 0.0))
    x, y, z, w = node.get("rotation", (0.0, 0.0, 0.0, 1.0))
    sx, sy, sz = node.get("scale", (1.0, 1.0, 1.0))
    rotation = [[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]]
    scale = (sx, sy, sz)
    return [[rotation[r][0] * scale[0], rotation[r][1] * scale[1], rotation[r][2] * scale[2], t]
            for r, t in enumerate((tx, ty, tz))] + [[0.0, 0.0, 0.0, 1.0]]


def normal_matrix(m):
    """The inverse transpose of the 3x3 part: normals stay normal to the surface under scaling."""
    a = [row[:3] for row in m[:3]]
    cof = [[a[(r + 1) % 3][(c + 1) % 3] * a[(r + 2) % 3][(c + 2) % 3] -
            a[(r + 1) % 3][(c + 2) % 3] * a[(r + 2) % 3][(c + 1) % 3] for c in range(3)] for r in range(3)]
    det = sum(a[0][c] * cof[0][c] for c in range(3))
    return [[cof[r][c] / det for c in range(3)] for r in range(3)] if abs(det) > 1e-12 else a


def transform(m, p):
    return [m[r][0] * p[0] + m[r][1] * p[1] + m[r][2] * p[2] + m[r][3] for r in range(3)]


def rotate(m, n):
    v = [m[r][0] * n[0] + m[r][1] * n[1] + m[r][2] * n[2] for r in range(3)]
    length = math.sqrt(sum(c * c for c in v)) or 1.0
    return [c / length for c in v]


# ---------------------------------------------------------------- conversion

def ascii_name(text, fallback):
    name = "".join(c if c.isascii() and (c.isalnum() or c in "-_") else "_" for c in text or "")
    return name or fallback


class Textures:
    """The textures of all the converted files: an image or a colour written once under one name."""

    def __init__(self, out, folder):
        self.out, self.folder = out, folder
        self.written = {}

    def name(self, key, make):
        if key not in self.written:
            stem = ascii_name(key[0], "texture")
            taken = set(self.written.values())
            name, n = stem, 1
            while f"{self.folder}\\{name}.tga" in taken:
                n += 1
                name = f"{stem}{n}"
            path = self.out / "textures" / self.folder / f"{name}.tga.tx"
            write_tx(make(), path)
            self.written[key] = f"{self.folder}\\{name}.tga"
        return self.written[key]


def material_texture(gltf, material, textures):
    """The engine texture of a material and the uv offset and scale of KHR_texture_transform."""
    pbr = material.get("pbrMetallicRoughness", {}) if material else {}
    factor = tuple(pbr.get("baseColorFactor", (1.0, 1.0, 1.0, 1.0)))
    info = pbr.get("baseColorTexture")
    if info is None:
        color = tuple(round(255 * min(max(c, 0.0), 1.0) ** (1 / 2.2)) for c in factor[:3]) + (255,)
        key = (f"color_{color[0]:02x}{color[1]:02x}{color[2]:02x}", color)
        return textures.name(key, lambda: Image.new("RGBA", (4, 4), color)), (0.0, 0.0), (1.0, 1.0)
    if info.get("texCoord", 0) != 0:
        warn(f"{gltf.path.name}: texture on uv set {info['texCoord']}, uv set 0 is used")
    image_index = gltf.json["textures"][info["index"]]["source"]
    image_name = gltf.json["images"][image_index].get("name") or Path(gltf.json["images"][image_index].get("uri", "texture")).stem
    transform_ext = info.get("extensions", {}).get("KHR_texture_transform", {})
    if transform_ext.get("rotation"):
        warn(f"{gltf.path.name}: texture rotation is not applied")

    def make():
        image = gltf.image(image_index)
        if factor != (1.0, 1.0, 1.0, 1.0):
            bands = [band.point(lambda v, f=f: round(v * f)) for band, f in zip(image.split(), factor)]
            image = Image.merge("RGBA", bands)
        return image

    # the same image tinted differently is a texture of its own
    key = (image_name, str(gltf.path.parent / image_name), factor)
    return (textures.name(key, make), tuple(transform_ext.get("offset", (0.0, 0.0))),
            tuple(transform_ext.get("scale", (1.0, 1.0))))


def primitive_mesh(gltf, primitive, matrix, name, textures, scale, single_sided):
    attributes = primitive["attributes"]
    positions = gltf.accessor(attributes["POSITION"])
    normals = gltf.accessor(attributes["NORMAL"]) if "NORMAL" in attributes else None
    uvs = gltf.accessor(attributes["TEXCOORD_0"]) if "TEXCOORD_0" in attributes else [(0.0, 0.0)] * len(positions)
    if "indices" in primitive:
        indices = [v[0] for v in gltf.accessor(primitive["indices"])]
    else:
        indices = list(range(len(positions)))
    material = gltf.json["materials"][primitive["material"]] if "material" in primitive else None
    texture, offset, uv_scale = material_texture(gltf, material, textures)

    normal_m = normal_matrix(matrix)
    # mirrored in x: the right-handed glTF model in the left-handed space of the engine
    world = [[-p[0] * scale, p[1] * scale, p[2] * scale] for p in (transform(matrix, p) for p in positions)]
    if normals is None:
        # flat normals: every triangle gets its own vertices
        world_tris = [[world[i] for i in indices[t:t + 3]] for t in range(0, len(indices) - 2, 3)]
        uvs = [uvs[i] for i in indices]
        world = [p for tri in world_tris for p in tri]
        world_normals = []
        for a, b, c in world_tris:
            u = [b[k] - a[k] for k in range(3)]
            v = [c[k] - a[k] for k in range(3)]
            # mirrored, so the right-handed counter-clockwise front gives the normal as c x b
            n = [u[2] * v[1] - u[1] * v[2], u[0] * v[2] - u[2] * v[0], u[1] * v[0] - u[0] * v[1]]
            length = math.sqrt(sum(x * x for x in n)) or 1.0
            world_normals += [[x / length for x in n]] * 3
        indices = list(range(len(world)))
    else:
        world_normals = [[-n[0], n[1], n[2]] for n in (rotate(normal_m, n) for n in normals)]

    mesh = Mesh(name, texture)
    for p, n, uv in zip(world, world_normals, uvs):
        mesh.vertex(p, n, (uv[0] * uv_scale[0] + offset[0], uv[1] * uv_scale[1] + offset[1]))
    double = bool(material and material.get("doubleSided")) and not single_sided
    first_back = len(mesh.vertices)
    if double:
        for p, n, uv in list(mesh.vertices[i][:3] for i in range(first_back)):
            mesh.vertex(p, [-c for c in n], uv)
    for t in range(0, len(indices) - 2, 3):
        a, b, c = indices[t:t + 3]
        if len({a, b, c}) < 3 or degenerate(mesh, a, b, c):
            continue
        mesh.triangle(a, b, c)
        if double:
            mesh.triangle(first_back + a, first_back + c, first_back + b)
    return mesh


def degenerate(mesh, a, b, c):
    pa, pb, pc = (mesh.vertices[i][0] for i in (a, b, c))
    u = [pb[k] - pa[k] for k in range(3)]
    v = [pc[k] - pa[k] for k in range(3)]
    n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
    return sum(x * x for x in n) < 1e-14


def convert(path, out, folder, textures, scale=1.0, bsp=True, single_sided=False):
    gltf = Gltf(path)
    doc = gltf.json
    scene = doc["scenes"][doc.get("scene", 0)] if doc.get("scenes") else {"nodes": list(range(len(doc["nodes"])))}
    meshes, names = [], set()

    def visit(index, parent):
        node = doc["nodes"][index]
        matrix = multiply(parent, node_matrix(node))
        if "skin" in node:
            warn(f"{gltf.path.name}: node {node.get('name', index)} is skinned, its bind pose is taken")
        if "mesh" in node:
            source = doc["meshes"][node["mesh"]]
            for p, primitive in enumerate(source["primitives"]):
                if primitive.get("mode", MODE_TRIANGLES) != MODE_TRIANGLES:
                    warn(f"{gltf.path.name}: {source.get('name', node['mesh'])} has lines or points, skipped")
                    continue
                base = ascii_name(node.get("name") or source.get("name"), f"mesh{len(meshes)}")
                name, n = base, 1
                while name in names:
                    n += 1
                    name = f"{base}{n}"
                names.add(name)
                mesh = primitive_mesh(gltf, primitive, matrix, name, textures, scale, single_sided)
                if mesh.triangles:
                    meshes.append(mesh)
        for child in node.get("children", []):
            visit(child, matrix)

    identity = [[1.0 if r == c else 0.0 for c in range(4)] for r in range(4)]
    for root in scene["nodes"]:
        visit(root, identity)
    if not meshes:
        raise ValueError(f"{gltf.path}: no triangles")
    vertices = sum(len(m.vertices) for m in meshes)
    if vertices >= 65536:
        raise ValueError(f"{gltf.path}: {vertices} vertices, a .gm takes 65535 (16-bit indices): split the model")
    stem = ascii_name(gltf.path.stem, "model")
    target = out / "models" / folder / f"{stem}.gm"
    write_gm(meshes, target, bsp=bsp)
    triangles = sum(len(m.triangles) for m in meshes)
    print(f"{target}: {len(meshes)} objects, {vertices} vertices, {triangles} triangles"
          f"{', bsp' if bsp else ''} -> load as \"{folder}\\{stem}\"")
    return target


def main():
    parser = argparse.ArgumentParser(description="glTF 2.0 (.glb, .gltf) to Storm Engine .gm and .tx")
    parser.add_argument("inputs", nargs="+", help="model files or folders of them")
    parser.add_argument("--out", required=True, help="the resource folder of the game or demo")
    parser.add_argument("--folder", default="imported", help="subfolder of models/ and textures/ (default: imported)")
    parser.add_argument("--scale", type=float, default=1.0, help="metres per unit of the model (default: 1)")
    parser.add_argument("--no-bsp", action="store_true", help="no BSP tree: traces miss the model, the file is smaller")
    parser.add_argument("--single-sided", action="store_true", help="ignore doubleSided, keep one face per triangle")
    args = parser.parse_args()

    files = []
    for item in map(Path, args.inputs):
        files += sorted(p for p in item.iterdir() if p.suffix.lower() in (".glb", ".gltf")) if item.is_dir() else [item]
    out = Path(args.out)
    textures = Textures(out, args.folder)
    failed = 0
    for path in files:
        try:
            convert(path, out, args.folder, textures, args.scale, not args.no_bsp, args.single_sided)
        except (ValueError, KeyError, OSError, struct.error) as error:
            warn(f"{path}: {error}")
            failed += 1
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
