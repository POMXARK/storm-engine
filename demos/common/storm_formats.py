"""Writers for the engine's native asset formats, shared by the demo generators.

- .tx  textures: A8R8G8B8 with the full mip chain; read_tx also reads the 16-bit and DXT ones
- .gm  geometry: static (RDF_VERTEX0) or skinned (RDF_AVERTEX0) meshes, one vertex buffer and
       optionally the BSP tree that traces and clips run against (shadows, cameras, shots)
- .an  skeletal animation: bone hierarchy, root positions and per-bone quaternions per frame
- .ptc character patch: the walkable surface of a location, its neighbours, lookup grid and paths

Meshes and skeletons are authored in model space as they should appear on screen; the writers take
care of the engine's conventions (see the notes at write_gm and write_an).
"""
import math
import struct
from collections import deque
from pathlib import Path

from PIL import Image

TXF_A8R8G8B8 = 21

# geometry/src/rdf.h
RDF_VERSION = (ord("1") << 24) | (ord(".") << 16) | (ord("0") << 8) | ord("5")
FLAGS_BSP_PRESENT = 2
OBJECT_VISIBLE = 1
TEXTURE_BASE = 1
VB_SKINNED = 4  # RDF_VERTEXBUFF.type bit: RDF_AVERTEX0 vertices, transformed on the CPU by the bones
# vertex colour is the albedo the lighting multiplies; the texture stage then applies MODULATE2X,
# so 0x80 grey keeps the texture as is under full light
VERTEX_COLOR = 0xFF808080


# ---------------------------------------------------------------- textures

def write_tx(image, path):
    """Writes an A8R8G8B8 texture with the full mip chain, the engine's native format."""
    image = image.convert("RGBA")
    width, height = image.size
    mips = []
    while True:
        r, g, b, a = image.split()
        mips.append(Image.merge("RGBA", (b, g, r, a)).tobytes())  # D3DFMT_A8R8G8B8 is BGRA in memory
        if image.width == 1 or image.height == 1:
            break
        image = image.resize((image.width // 2, image.height // 2), Image.LANCZOS)
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "wb") as out:
        out.write(struct.pack("<6i", 0, width, height, len(mips), TXF_A8R8G8B8, len(mips[0])))
        for mip in mips:
            out.write(mip)


# renderer/src/texture.h: uncompressed formats as (bytes per pixel, Pillow raw mode); DXT ones as
# the BCn decoder number of Pillow
TX_RAW = {21: (4, "BGRA"), 22: (4, "BGRX"), 23: (2, "BGR;16"), 25: (2, "BGRA;15"), 26: (2, None)}
TX_DXT = {fourcc_le: n for fourcc_le, n in ((int.from_bytes(b"DXT1", "little"), 1),
                                           (int.from_bytes(b"DXT3", "little"), 2),
                                           (int.from_bytes(b"DXT5", "little"), 3))}
TX_FLAGS_PALETTE = 1
TX_FLAGS_CUBEMAP = 2


def read_tx(path):
    """Reads the largest mip of a .tx (of a cubemap: its first face, +Z) as an RGBA image."""
    data = Path(path).read_bytes()
    flags, width, height, nmips, fmt, mip_size = struct.unpack_from("<6i", data)
    if flags & TX_FLAGS_PALETTE:
        raise ValueError("palette textures are not loaded by the engine either")
    pixels = data[24:24 + mip_size]
    if fmt in TX_DXT:
        return Image.frombytes("RGBA", (width, height), pixels, "bcn", TX_DXT[fmt]).convert("RGBA")
    if fmt not in TX_RAW:
        raise ValueError(f"unsupported .tx format {fmt:#x}")
    size, mode = TX_RAW[fmt]
    if mode is None:
        # A4R4G4B4, which Pillow has no raw mode for: the 16-bit value holds a, r, g, b nibbles from
        # the top; each nibble x17 gives 8 bits
        pixels = bytes(17 * (byte >> shift & 15) for i in range(0, len(pixels), 2)
                       for byte, shift in ((pixels[i + 1], 0), (pixels[i], 4), (pixels[i], 0), (pixels[i + 1], 4)))
        mode = "RGBA"
    rgb = "RGB" if mode in ("BGRX", "BGR;16") else "RGBA"
    return Image.frombytes(rgb, (width, height), pixels, "raw", mode).convert("RGBA")


def tx_info(path):
    flags, width, height, nmips, fmt, mip_size = struct.unpack_from("<6i", Path(path).read_bytes())
    names = {21: "A8R8G8B8", 22: "X8R8G8B8", 23: "R5G6B5", 25: "A1R5G5B5", 26: "A4R4G4B4"}
    name = names.get(fmt) or fmt.to_bytes(4, "little").decode("ascii", "replace")
    kind = " cubemap" if flags & TX_FLAGS_CUBEMAP else ""
    return f"{width}x{height} {name}{kind}, {nmips} mips"


# ---------------------------------------------------------------- geometry

class Mesh:
    """One .gm object: vertices (pos, normal, uv, bone) and triangles facing along the vertex normals."""

    def __init__(self, name, texture):
        self.name = name
        self.texture = texture
        self.vertices = []
        self.triangles = []

    def vertex(self, pos, normal, uv, bone=0):
        self.vertices.append((tuple(pos), tuple(normal), tuple(uv), bone))
        return len(self.vertices) - 1

    def triangle(self, a, b, c):
        # D3D culls counter-clockwise faces by default: in its left-handed space a front face has
        # cross(b - a, c - a) pointing to the viewer, so orient every triangle along its normals
        pa, pb, pc = (self.vertices[i][0] for i in (a, b, c))
        e1 = [pb[k] - pa[k] for k in range(3)]
        e2 = [pc[k] - pa[k] for k in range(3)]
        cross = (e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0])
        normal = [sum(self.vertices[i][1][k] for i in (a, b, c)) for k in range(3)]
        if sum(cross[k] * normal[k] for k in range(3)) < 0:
            b, c = c, b
        self.triangles.append((a, b, c))

    def quad(self, a, b, c, d):
        self.triangle(a, b, c)
        self.triangle(a, c, d)


def mirror_x(v):
    return (-v[0], v[1], v[2])


def write_gm(meshes, path, skinned=False, bsp=False):
    """Writes a .gm: one material and texture per mesh, one vertex buffer and, with bsp, the tree
    GEOM::Trace and GEOM::Clip search (without it every trace misses the model).

    Skinned meshes get RDF_AVERTEX0 vertices bound to one bone each. The engine's skinning
    (model.cpp, VBTransform) negates x of every transformed vertex, so they are stored mirrored
    in x; triangle order is kept, so after that mirror they face the way they were authored.
    """
    strings = bytearray()
    offsets = {}

    def name(text):
        if text not in offsets:
            offsets[text] = len(strings)
            strings.extend(text.encode("ascii") + b"\0")
        return offsets[text]

    store = mirror_x if skinned else (lambda v: v)

    group = name("geometry")
    textures = []
    for mesh in meshes:
        if mesh.texture not in textures:
            textures.append(mesh.texture)
    texture_names = [name(t) for t in textures]
    object_names = [name(m.name) for m in meshes]
    material_names = [name(m.name + "_mtl") for m in meshes]

    all_pos = [store(v[0]) for m in meshes for v in m.vertices]
    lo = [min(p[k] for p in all_pos) for k in range(3)]
    hi = [max(p[k] for p in all_pos) for k in range(3)]
    bbox_size = [hi[k] - lo[k] for k in range(3)]
    bbox_center = [(hi[k] + lo[k]) / 2 for k in range(3)]
    radius = max(math.dist(p, bbox_center) for p in all_pos)

    materials = b""
    objects = b""
    triangles = b""
    vertices = b""
    striangle = svertex = 0
    for o, mesh in enumerate(meshes):
        texture_type = [TEXTURE_BASE, 0, 0, 0]
        texture = [textures.index(mesh.texture), 0, 0, 0]
        materials += struct.pack("<2i4f4i4i", group, material_names[o], 1.0, 0.0, 1.0, 0.0, *texture_type, *texture)

        pos = [store(v[0]) for v in mesh.vertices]
        lo_o = [min(p[k] for p in pos) for k in range(3)]
        hi_o = [max(p[k] for p in pos) for k in range(3)]
        center = [(hi_o[k] + lo_o[k]) / 2 for k in range(3)]
        obj_radius = max(math.dist(p, center) for p in pos)
        if skinned:
            # the bounds are for clipping and the bones move the vertices: take the whole model
            center, obj_radius = bbox_center, radius * 1.5
        objects += struct.pack("<3i3ff6i8i4ii", group, object_names[o], OBJECT_VISIBLE, *center, obj_radius,
                               0, len(mesh.triangles), striangle, len(mesh.vertices), svertex, o,
                               *([-1] * 8), *([-1] * 4), 0)
        # indices are relative to the object's first vertex (DrawIndexedPrimitive BaseVertexIndex)
        for tri in mesh.triangles:
            triangles += struct.pack("<3H", *tri)
        for p, n, uv, bone in mesh.vertices:
            if skinned:
                # weight applies to the bone in the low byte, 1 - weight to the next byte
                vertices += struct.pack("<3ffI3fI2f", *store(p), 1.0, bone | bone << 8, *store(n),
                                        VERTEX_COLOR, *uv)
            else:
                vertices += struct.pack("<3f3fI2f", *p, *n, VERTEX_COLOR, *uv)
        striangle += len(mesh.triangles)
        svertex += len(mesh.vertices)
    assert svertex < 65536, "16-bit indices"

    names = list(offsets.values())
    flags = 0
    if bsp:
        assert not skinned, "the BSP is in model space, bones would move the triangles away from it"
        flags |= FLAGS_BSP_PRESENT
        # one list of positions and triangles in object order: a hit is mapped back to its object
        # by counting triangles (GEOM::GetCollisionDetails)
        bsp_vertices, bsp_triangles = [], []
        for mesh in meshes:
            base = len(bsp_vertices)
            bsp_vertices += [v[0] for v in mesh.vertices]
            bsp_triangles += [tuple(base + i for i in tri) for tri in mesh.triangles]
    head = struct.pack("<11i3f3ff", RDF_VERSION, flags, len(strings), len(names), len(textures), len(meshes), 0, 0,
                       len(meshes), striangle, 1, *bbox_size, *bbox_center, radius)
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "wb") as out:
        out.write(head)
        out.write(bytes(strings))
        out.write(struct.pack(f"<{len(names)}i", *names))
        out.write(struct.pack(f"<{len(textures)}i", *texture_names))
        out.write(materials)
        out.write(objects)
        out.write(triangles)
        out.write(struct.pack("<2i", VB_SKINNED if skinned else 0, len(vertices)))  # RDF_VERTEXBUFF
        out.write(vertices)
        if bsp:
            out.write(bsp_tree(bsp_vertices, bsp_triangles))


# BSP_NODE (rdf.h): plane, then a bit field with the children and the face count, then 3-byte
# triangle indices that start in the last field and run on into the following array slots
BSP_NODE_SIZE = 24
BSP_MAX_FACES = 15
BSP_MAX_DEPTH = 250  # GEOM::Trace and GEOM::Clip keep 256-entry stacks
BSP_EPSILON = 1e-4


class _BspNode:
    def __init__(self, normal, distance, faces):
        self.normal, self.distance, self.faces = normal, distance, faces
        self.left = self.right = None

    def slots(self):
        return 1 + max(0, -(-(3 * len(self.faces) - 4) // BSP_NODE_SIZE))


def bsp_tree(positions, triangles):
    """Builds the BSP block of a .gm: RDF_BSPHEAD, nodes, vertices, triangles.

    Every node splits by the plane of some triangles and keeps up to 15 of those coplanar faces;
    the rest go to the negative ("left") or positive ("right") child, triangles crossing the
    plane to both. Trace walks the near side, tests the node faces where the segment crosses the
    plane, then the far side, so the first hit is the nearest one.
    """
    def dot(a, b):
        return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]

    planes = []
    for tri in triangles:
        a, b, c = (positions[i] for i in tri)
        u = [b[k] - a[k] for k in range(3)]
        v = [c[k] - a[k] for k in range(3)]
        n = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]]
        length = math.sqrt(dot(n, n))
        assert length > 0, f"degenerate triangle {tri}"
        n = [x / length for x in n]
        # faces either way round share a plane: the face test is two-sided
        if next(x for x in n if abs(x) > 1e-6) < 0:
            n = [-x for x in n]
        planes.append((tuple(n), dot(n, a)))

    def classify(t, normal, distance):
        side = [dot(normal, positions[i]) - distance for i in triangles[t]]
        return min(side) < -BSP_EPSILON, max(side) > BSP_EPSILON

    def build(tris, depth):
        if not tris:
            return None
        assert depth < BSP_MAX_DEPTH, "BSP too deep"
        groups = {}
        for t in tris:
            n, d = planes[t]
            groups.setdefault(tuple(round(x, 4) for x in n) + (round(d, 4),), []).append(t)
        # a few candidate planes, the one splitting fewest triangles and halving the rest wins
        keys = sorted(groups)
        step = max(1, len(keys) // 12)
        best = None
        for key in keys[::step]:
            normal, distance = planes[groups[key][0]]
            left = right = both = 0
            for t in tris:
                neg, pos = classify(t, normal, distance)
                left += neg and not pos
                right += pos and not neg
                both += neg and pos
            score = 8 * both + abs(left - right)
            if best is None or score < best[0]:
                best = score, key
        faces = groups[best[1]][:BSP_MAX_FACES]
        node = _BspNode(*planes[faces[0]], faces)
        left, right = [], []
        taken = set(faces)
        for t in tris:
            if t in taken:
                continue
            neg, pos = classify(t, node.normal, node.distance)
            if neg:
                left.append(t)
            # coplanar faces beyond the 15 go on in the positive child, which meets them at the
            # same crossing distance
            if pos or not neg:
                right.append(t)
        node.left, node.right = build(left, depth + 1), build(right, depth + 1)
        return node

    root = build(list(range(len(triangles))), 0)

    # the root is slot 0; the children of a node lie side by side, left first, so the right one
    # is found as an offset of 1..3 slots (BSP_NODE.right is 2 bits)
    position = {id(root): 0}
    size = root.slots()
    order = [root]
    for node in order:
        for child in (node.left, node.right):
            if child:
                position[id(child)] = size
                size += child.slots()
                order.append(child)
    assert size < 1 << 22
    nodes = bytearray(size * BSP_NODE_SIZE)
    for node in order:
        if node.left:
            first, has_left = position[id(node.left)], 1
            right = position[id(node.right)] - first if node.right else 0
        else:
            has_left, right = 0, 1 if node.right else 0
            first = position[id(node.right)] - 1 if node.right else 0
        assert right <= 3
        bits = first | has_left << 23 | len(node.faces) << 24 | right << 28
        faces = b"".join(t.to_bytes(3, "little") for t in node.faces)
        record = struct.pack("<3ffI", *node.normal, node.distance, bits) + faces
        offset = position[id(node)] * BSP_NODE_SIZE
        nodes[offset:offset + len(record)] = record

    out = struct.pack("<3i", size, len(positions), len(triangles)) + bytes(nodes)
    out += b"".join(struct.pack("<3f", *p) for p in positions)
    out += b"".join(i.to_bytes(3, "little") for tri in triangles for i in tri)
    return out


# ---------------------------------------------------------------- animation

def quat_axis_angle(axis, angle):
    """Quaternion (x, y, z, w) rotating by angle around axis, as Quaternion::GetMatrix reads it."""
    s = math.sin(angle / 2) / math.sqrt(sum(c * c for c in axis))
    return (axis[0] * s, axis[1] * s, axis[2] * s, math.cos(angle / 2))


def quat_mul(a, b):
    """Rotation a, then b (the order of row-vector matrices: v * A * B)."""
    ax, ay, az, aw = b
    bx, by, bz, bw = a
    return (aw * bx + ax * bw + ay * bz - az * by,
            aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw,
            aw * bw - ax * bx - ay * by - az * bz)


def quat_rotate(q, v):
    """v rotated by q, the same as v * Quaternion::GetMatrix(q)."""
    x, y, z, w = q
    m = ((1 - 2 * (y * y + z * z), 2 * (x * y + w * z), 2 * (x * z - w * y)),
         (2 * (x * y - w * z), 1 - 2 * (x * x + z * z), 2 * (y * z + w * x)),
         (2 * (x * z + w * y), 2 * (y * z - w * x), 1 - 2 * (x * x + y * y)))
    return tuple(v[0] * m[0][k] + v[1] * m[1][k] + v[2] * m[2][k] for k in range(3))


def write_an(path, fps, parents, joints, root_positions, rotations):
    """Writes an .an file (animation/src/an_file.h, AnimationServiceImp::LoadAN).

    parents[i]        parent bone of bone i (bone 0 is the root, its entry is ignored)
    joints[i]         joint position relative to the parent's joint, in the bind pose
    root_positions[f] position of the root joint in frame f
    rotations[i][f]   rotation of bone i relative to its parent in frame f

    Frame 0 is the bind pose the mesh is modelled in (the engine builds the inverse bind matrices
    from it), so the mesh must match frame 0. Like the meshes, everything is stored mirrored in x
    for the skinning's x negation: positions get -x, quaternions (x, -y, -z, w).
    """
    frames = len(root_positions)
    assert all(len(r) == frames for r in rotations)
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "wb") as out:
        out.write(struct.pack("<iif", frames, len(parents), fps))
        out.write(struct.pack(f"<{len(parents)}i", *[-1] + list(parents[1:])))
        for joint in joints:
            out.write(struct.pack("<3f", *mirror_x(joint)))
        for pos in root_positions:
            out.write(struct.pack("<3f", *mirror_x(pos)))
        for bone in rotations:
            for x, y, z, w in bone:
                out.write(struct.pack("<4f", x, -y, -z, w))


# ---------------------------------------------------------------- character patch

def fourcc(text):
    """A multi-character constant as MSVC builds it: 'abcd' is (a << 24) | (b << 16) | (c << 8) | d."""
    value = 0
    for ch in text:
        value = value << 8 | ord(ch)
    return value


PTC_ID = fourcc(" CTP")
PTC_VERSION = fourcc("10.1")
PTC_SPACE = fourcc("    ")
PTC_NOSEA = fourcc(" HTE")
PTC_UNREACHABLE = 3


def write_ptc(triangles, path, materials=("ground",), cell=1.0):
    """Writes a .ptc character patch (location/include/ptc.h, PtcData::Load).

    triangles: ((p0, p1, p2), material index) in world space. Characters walk on these
    triangles only: an edge two triangles share by both vertices is a passage, any other edge
    is a wall the character slides along. So a step down without shared vertices is a wall too.

    - vertices are merged by position, which is what makes the shared edges
    - PtcData::FindNode treats a point as inside when it is on the left of every edge seen from
      above, so triangles are wound with cross(b - a, c - a).y > 0; their normals point up
    - nb[j] is the neighbour across the edge i[j] -> i[j + 1], -1 for a wall
    - the lookup grid splits the bounds into cells of about `cell` metres, each listing the
      triangles whose bounds overlap it
    - the path table has a row per triangle with 2 bits per destination triangle: the edge to
      leave through to get there by the fewest triangles (breadth-first), 3 if unreachable
    """
    vertex_index = {}
    vertices = []
    tris = []
    for points, material in triangles:
        ids = []
        for p in points:
            key = tuple(round(c, 4) for c in p)
            if key not in vertex_index:
                vertex_index[key] = len(vertices)
                vertices.append(key)
            ids.append(vertex_index[key])
        a, b, c = (vertices[i] for i in ids)
        cross_y = (b[2] - a[2]) * (c[0] - a[0]) - (b[0] - a[0]) * (c[2] - a[2])
        assert abs(cross_y) > 1e-9, "a vertical or degenerate triangle cannot be walked on"
        if cross_y < 0:
            ids[1], ids[2] = ids[2], ids[1]
        tris.append((ids, material))
    count = len(tris)
    assert count < 32768 and len(vertices) < 65536

    normals = []
    for ids, _ in tris:
        a, b, c = (vertices[i] for i in ids)
        e1 = [b[k] - a[k] for k in range(3)]
        e2 = [c[k] - a[k] for k in range(3)]
        n = (e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0])
        length = math.sqrt(sum(x * x for x in n))
        normals.append(tuple(x / length for x in n))

    edges = {}
    for t, (ids, _) in enumerate(tris):
        for j in range(3):
            edges.setdefault(frozenset((ids[j], ids[(j + 1) % 3])), []).append(t)
    neighbours = []
    for t, (ids, _) in enumerate(tris):
        nb = []
        for j in range(3):
            others = [o for o in edges[frozenset((ids[j], ids[(j + 1) % 3]))] if o != t]
            assert len(others) <= 1, "an edge shared by more than two triangles"
            nb.append(others[0] if others else -1)
        neighbours.append(nb)

    lo = [min(v[k] for v in vertices) for k in range(3)]
    hi = [max(v[k] for v in vertices) for k in range(3)]
    map_w = max(1, math.ceil((hi[0] - lo[0]) / cell))
    map_l = max(1, math.ceil((hi[2] - lo[2]) / cell))
    ws = (hi[0] - lo[0]) / map_w
    ls = (hi[2] - lo[2]) / map_l
    grid = [[] for _ in range(map_w * map_l)]
    for t, (ids, _) in enumerate(tris):
        xs = [vertices[i][0] for i in ids]
        zs = [vertices[i][2] for i in ids]
        x0 = max(0, int((min(xs) - lo[0]) / ws))
        x1 = min(map_w - 1, int((max(xs) - lo[0]) / ws))
        z0 = max(0, int((min(zs) - lo[2]) / ls))
        z1 = min(map_l - 1, int((max(zs) - lo[2]) / ls))
        for mz in range(z0, z1 + 1):
            for mx in range(x0, x1 + 1):
                grid[mz * map_w + mx].append(t)
    indices = [t for cell_list in grid for t in cell_list]

    # one breadth-first search per destination fills its column of the table
    line_size = (count + 3) // 4
    table = bytearray(b"\xff" * (line_size * count))
    for to in range(count):
        seen = {to}
        queue = deque([to])
        while queue:
            u = queue.popleft()
            for n in neighbours[u]:
                if n < 0 or n in seen:
                    continue
                seen.add(n)
                queue.append(n)
                edge = neighbours[n].index(u)
                byte = n * line_size + (to >> 2)
                shift = (to & 3) * 2
                table[byte] = (table[byte] & ~(3 << shift)) | (edge << shift)

    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "wb") as out:
        out.write(struct.pack("<13i6f", PTC_ID, PTC_SPACE, PTC_VERSION, PTC_SPACE, PTC_NOSEA, PTC_SPACE,
                              count, len(vertices), len(normals), map_l, map_w, len(indices), line_size, *lo, *hi))
        for t, (ids, material) in enumerate(tris):
            out.write(struct.pack("<4H3hbb", *ids, t, *neighbours[t], material, 0))
        for v in vertices:
            out.write(struct.pack("<3f", *v))
        for n in normals:
            out.write(struct.pack("<3f", *n))
        start = 0
        for cell_list in grid:
            out.write(struct.pack("<2i", start, len(cell_list)))
            start += len(cell_list)
        out.write(struct.pack(f"<{len(indices)}H", *indices))
        out.write(bytes(table))
        assert len(materials) <= 16
        out.write(struct.pack("<i", len(materials)))
        for name in list(materials) + [""] * (16 - len(materials)):
            out.write(name.encode("ascii")[:15].ljust(16, b"\0"))
    return count
