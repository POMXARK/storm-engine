"""Writers for the engine's native asset formats, shared by the demo generators.

- .tx  textures: A8R8G8B8 with the full mip chain
- .gm  geometry: static (RDF_VERTEX0) or skinned (RDF_AVERTEX0) meshes, one vertex buffer, no BSP
- .an  skeletal animation: bone hierarchy, root positions and per-bone quaternions per frame

Meshes and skeletons are authored in model space as they should appear on screen; the writers take
care of the engine's conventions (see the notes at write_gm and write_an).
"""
import math
import struct
from pathlib import Path

from PIL import Image

TXF_A8R8G8B8 = 21

# geometry/src/rdf.h
RDF_VERSION = (ord("1") << 24) | (ord(".") << 16) | (ord("0") << 8) | ord("5")
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


def write_gm(meshes, path, skinned=False):
    """Writes a .gm: one material and texture per mesh, one vertex buffer, no BSP.

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
    head = struct.pack("<11i3f3ff", RDF_VERSION, 0, len(strings), len(names), len(textures), len(meshes), 0, 0,
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
