"""A generated sailor shared by the demos: skinned mesh, textures, skeleton and looped animation cycles.

The sailor is a low-poly mannequin: every part is rigidly bound to one of 11 bones. Frame 0 of the
.an file is the bind pose the mesh is modelled in; the cycles ("idle", "walk", "run", "turn") follow it.
"""
import math

from PIL import Image, ImageDraw, ImageFilter

from storm_formats import Mesh, quat_axis_angle, quat_mul, quat_rotate, write_an, write_gm, write_tx

FPS = 30.0

# ---------------------------------------------------------------- skeleton
# model space: y up, the sailor faces +z; joint positions in the bind pose
BONES = [
    # name,        parent, joint
    ("pelvis", -1, (0.0, 0.95, 0.0)),
    ("chest", 0, (0.0, 1.12, 0.0)),
    ("neck", 1, (0.0, 1.46, 0.0)),
    ("shoulder_l", 1, (0.215, 1.41, 0.0)),
    ("elbow_l", 3, (0.235, 1.13, 0.0)),
    ("shoulder_r", 1, (-0.215, 1.41, 0.0)),
    ("elbow_r", 5, (-0.235, 1.13, 0.0)),
    ("hip_l", 0, (0.095, 0.92, 0.0)),
    ("knee_l", 7, (0.1, 0.5, 0.0)),
    ("hip_r", 0, (-0.095, 0.92, 0.0)),
    ("knee_r", 9, (-0.1, 0.5, 0.0)),
]
BONE = {name: i for i, (name, _, _) in enumerate(BONES)}
PARENTS = [parent for _, parent, _ in BONES]
JOINTS = [joint for _, _, joint in BONES]

X_AXIS, Y_AXIS = (1, 0, 0), (0, 1, 0)
IDENTITY = (0.0, 0.0, 0.0, 1.0)


# ---------------------------------------------------------------- textures

def noisy(size, color, amount=14):
    image = Image.new("RGB", size, color)
    noise = Image.effect_noise(size, amount).convert("RGB")
    return Image.blend(image, Image.composite(image, noise, Image.new("L", size, 215)), 0.5)


def stripes(color=(28, 40, 92)):
    # the tel'nyashka: one white and one navy stripe per tile, v follows the height
    image = Image.new("RGB", (64, 64), (236, 236, 230))
    ImageDraw.Draw(image).rectangle([0, 32, 63, 63], fill=color)
    return image.filter(ImageFilter.GaussianBlur(0.7))


def face():
    # u runs around the head (0.5 is the front), v from the crown down
    image = noisy((256, 128), (222, 170, 128), 8)
    draw = ImageDraw.Draw(image)
    for du in (-14, 14):
        x = 128 + du
        draw.ellipse([x - 5, 58, x + 5, 66], fill=(245, 240, 232))
        draw.ellipse([x - 3, 59, x + 3, 65], fill=(52, 36, 26))
        draw.line([(x - 7, 52), (x + 7, 51)], fill=(70, 46, 30), width=3)
    draw.line([(124, 66), (122, 80), (127, 81)], fill=(176, 122, 90), width=2)
    draw.line([(119, 89), (137, 89)], fill=(140, 70, 60), width=3)
    return image.filter(ImageFilter.GaussianBlur(0.6))


def write_textures(directory, prefix="", stripe=(28, 40, 92), trousers=(44, 52, 76), hair=(58, 40, 28)):
    """<prefix>sailor_*.tga.tx into directory; the meshes refer to them by texture_prefix + name.
    The colours make another man of the same mesh (a pirate in red stripes)."""
    textures = {
        "sailor_skin": noisy((64, 64), (222, 170, 128), 8),
        "sailor_face": face(),
        "sailor_hair": noisy((64, 64), hair, 30),
        "sailor_shirt": stripes(stripe),
        "sailor_trousers": noisy((64, 64), trousers, 24),
        "sailor_boots": noisy((64, 64), (58, 40, 30), 20),
    }
    for name, image in textures.items():
        write_tx(image, directory / f"{prefix}{name}.tga.tx")


# ---------------------------------------------------------------- geometry

def ellipse_cylinder(mesh, bone, x, z, y_top, y_bottom, r_top, r_bottom, depth=1.0, v_scale=None,
                     segments=12, caps=True):
    """A closed elliptical cylinder; radius r in x, r * depth in z. u = 0.5 faces +z.
    v_scale: v grows with height by this factor (tiling textures), else 0..1 top to bottom."""
    rows = []
    for j, (y, r) in enumerate(((y_top, r_top), (y_bottom, r_bottom))):
        row = []
        for i in range(segments + 1):
            a = 2 * math.pi * (i / segments - 0.5)
            px, pz = r * math.sin(a), r * depth * math.cos(a)
            nx, nz = math.sin(a) / r, math.cos(a) / (r * depth)
            length = math.hypot(nx, nz)
            v = (2.0 - y) * v_scale if v_scale else j
            row.append(mesh.vertex((x + px, y, z + pz), (nx / length, 0, nz / length), (i / segments, v), bone))
        rows.append(row)
    for i in range(segments):
        mesh.quad(rows[0][i], rows[0][i + 1], rows[1][i + 1], rows[1][i])
    if caps:
        for (y, r), ny in (((y_top, r_top), 1.0), ((y_bottom, r_bottom), -1.0)):
            center = mesh.vertex((x, y, z), (0, ny, 0), (0.5, 0.5), bone)
            ring = [mesh.vertex((x + r * math.sin(a), y, z + r * depth * math.cos(a)), (0, ny, 0),
                                (0.5 + 0.5 * math.sin(a), 0.5 + 0.5 * math.cos(a)), bone)
                    for a in (2 * math.pi * i / segments for i in range(segments + 1))]
            for i in range(segments):
                mesh.triangle(center, ring[i], ring[i + 1])


def ellipsoid(mesh, bone, center, radii, segments=14, rings=10, max_polar=math.pi):
    """An ellipsoid (or its top part down to max_polar); u = 0.5 faces +z, v from the top down."""
    grid = []
    for j in range(rings + 1):
        polar = max_polar * j / rings
        row = []
        for i in range(segments + 1):
            a = 2 * math.pi * (i / segments - 0.5)
            d = (math.sin(polar) * math.sin(a), math.cos(polar), math.sin(polar) * math.cos(a))
            pos = tuple(center[k] + radii[k] * d[k] for k in range(3))
            n = tuple(d[k] / radii[k] for k in range(3))
            length = math.sqrt(sum(c * c for c in n))
            row.append(mesh.vertex(pos, tuple(c / length for c in n), (i / segments, polar / math.pi), bone))
        grid.append(row)
    for j in range(rings):
        for i in range(segments):
            mesh.quad(grid[j][i], grid[j][i + 1], grid[j + 1][i + 1], grid[j + 1][i])


def box(mesh, bone, lo, hi, uv_scale=1.0):
    """An axis-aligned box; with uv_scale the texture tiles once per 1 / uv_scale metres."""
    for axis in range(3):
        for sign in (0, 1):
            n = [0, 0, 0]
            n[axis] = 1 if sign else -1
            u_axis, v_axis = [k for k in range(3) if k != axis]
            corners = []
            for du, dv in ((0, 0), (1, 0), (1, 1), (0, 1)):
                p = [0.0, 0.0, 0.0]
                p[axis] = hi[axis] if sign else lo[axis]
                p[u_axis] = hi[u_axis] if du else lo[u_axis]
                p[v_axis] = hi[v_axis] if dv else lo[v_axis]
                uv = (p[u_axis] * uv_scale, -p[v_axis] * uv_scale) if uv_scale != 1.0 else (du, dv)
                corners.append(mesh.vertex(p, n, uv, bone))
            mesh.quad(*corners)


def meshes(texture_prefix):
    def mesh(name):
        return Mesh(name, f"{texture_prefix}sailor_{name}.tga")

    skin, head, hair = mesh("skin"), Mesh("head", f"{texture_prefix}sailor_face.tga"), mesh("hair")
    shirt, trousers, boots = mesh("shirt"), mesh("trousers"), mesh("boots")

    ellipse_cylinder(trousers, BONE["pelvis"], 0, 0, 1.02, 0.83, 0.165, 0.165, depth=0.68, v_scale=4)
    ellipse_cylinder(shirt, BONE["chest"], 0, 0, 1.44, 1.0, 0.19, 0.16, depth=0.62, v_scale=16, segments=16)
    ellipse_cylinder(skin, BONE["neck"], 0, 0, 1.53, 1.42, 0.05, 0.052, v_scale=4, caps=False)
    ellipsoid(head, BONE["neck"], (0, 1.63, 0.005), (0.1, 0.12, 0.11))
    ellipsoid(hair, BONE["neck"], (0, 1.64, -0.004), (0.106, 0.124, 0.116), rings=6, max_polar=1.25)
    for side, sx in (("l", 1), ("r", -1)):
        ellipse_cylinder(shirt, BONE["shoulder_" + side], sx * 0.225, 0, 1.46, 1.11, 0.054, 0.046, v_scale=16)
        ellipse_cylinder(skin, BONE["elbow_" + side], sx * 0.235, 0, 1.15, 0.89, 0.042, 0.035, v_scale=4)
        ellipsoid(skin, BONE["elbow_" + side], (sx * 0.235, 0.84, 0.01), (0.036, 0.06, 0.045), segments=8, rings=6)
        ellipse_cylinder(trousers, BONE["hip_" + side], sx * 0.097, 0, 0.97, 0.47, 0.08, 0.062, v_scale=4)
        ellipse_cylinder(trousers, BONE["knee_" + side], sx * 0.1, 0, 0.52, 0.12, 0.06, 0.047, v_scale=4)
        box(boots, BONE["knee_" + side], (sx * 0.1 - 0.055, 0.0, -0.07), (sx * 0.1 + 0.055, 0.14, 0.16))
    return [skin, head, hair, shirt, trousers, boots]


# ---------------------------------------------------------------- animation
# rotations are relative to the parent bone; around x a positive angle turns +y towards +z,
# so a leg (pointing down) swings forward with a negative angle

def rot_x(angle):
    return quat_axis_angle(X_AXIS, angle)


def rot_y(angle):
    return quat_axis_angle(Y_AXIS, angle)


def rot_z(angle):
    """around z a positive angle turns +x towards +y: the left arm (+x) rises sideways"""
    return quat_axis_angle((0, 0, 1), angle)


def euler(x=0.0, y=0.0, z=0.0, twist=0.0):
    """A bone rotation: twist about the bone's own y axis first (a forearm turning the hand), then
    x (forward / back), z (sideways) and y (turning around the vertical) in the parent's frame."""
    return quat_mul(quat_mul(quat_mul(rot_y(twist), rot_x(x)), rot_z(z)), rot_y(y))


def idle_pose(t):
    p = 2 * math.pi * t
    rot = {
        "chest": rot_x(0.025 * math.sin(2 * p)),  # breathing, two breaths a cycle
        "neck": quat_mul(rot_x(0.05), rot_y(0.35 * math.sin(p))),  # looks around
        "shoulder_l": rot_x(0.04 * math.sin(p)),
        "shoulder_r": rot_x(-0.04 * math.sin(p)),
        "elbow_l": rot_x(-0.15),
        "elbow_r": rot_x(-0.15),
    }
    root = (0.0, JOINTS[0][1] - 0.004 * (1 - math.cos(2 * p)), 0.0)
    return root, rot


def gait_pose(t, hip, knee, arm, elbow, lean, bounce):
    """One stride of both legs; the left leg leads at t = 0.25."""
    p = 2 * math.pi * t
    s, c = math.sin(p), math.cos(p)
    rot = {
        # the pelvis turns with the leading leg, the chest against it
        "pelvis": rot_y(-0.07 * s),
        "chest": quat_mul(rot_x(lean), rot_y(0.13 * s)),
        "neck": quat_mul(rot_x(-lean * 0.7), rot_y(-0.06 * s)),
        "hip_l": rot_x(-hip * s),
        "hip_r": rot_x(hip * s),
        # a knee bends while its leg swings forward
        "knee_l": rot_x(0.1 + knee * max(0.0, c)),
        "knee_r": rot_x(0.1 + knee * max(0.0, -c)),
        # arms swing against the legs
        "shoulder_l": rot_x(arm * s),
        "shoulder_r": rot_x(-arm * s),
        "elbow_l": rot_x(-elbow - 0.15 * max(0.0, -s)),
        "elbow_r": rot_x(-elbow - 0.15 * max(0.0, s)),
    }
    # lowest when the legs are spread
    root = (0.0, JOINTS[0][1] - bounce * s * s, 0.0)
    return root, rot


def walk_pose(t):
    return gait_pose(t, hip=0.45, knee=0.75, arm=0.35, elbow=0.3, lean=0.06, bounce=0.03)


def run_pose(t):
    return gait_pose(t, hip=0.7, knee=1.3, arm=0.6, elbow=1.2, lean=0.2, bounce=0.06)


def turn_pose(t):
    """Stepping in place while turning on the spot."""
    return gait_pose(t, hip=0.12, knee=0.5, arm=0.06, elbow=0.3, lean=0.02, bounce=0.01)


class Cycle:
    """An action: frames of one cycle and the distance it covers (0 for standing ones). A looped
    one ends on its first pose; with loop=False it plays its pose from t = 0 to t = 1 once."""

    def __init__(self, name, pose, frames, stride=0.0, loop=True):
        self.name, self.pose, self.frames, self.stride, self.loop = name, pose, frames, stride, loop
        self.start = self.end = None

    @property
    def speed(self):
        """metres per second the model must move for the feet not to slide"""
        return self.stride / (self.frames / FPS)


IDLE = Cycle("idle", idle_pose, 120)  # 4 s
WALK = Cycle("walk", walk_pose, 32, stride=1.4)  # two steps of 0.7 m
RUN = Cycle("run", run_pose, 20, stride=2.4)  # two strides of 1.2 m
TURN = Cycle("turn", turn_pose, 24)


def frame_list(cycles):
    """(root position, {bone: rotation}) for every frame of the .an file; sets cycle.start/end.

    Frame 0 is the bind pose; every looped cycle repeats its first frame at the end (the loop
    point), a single one ends on its last pose; both are followed by one spare frame, because the
    engine blends frame f with f + 1.
    """
    frames = [(JOINTS[0], {})]
    for cycle in cycles:
        cycle.start = len(frames)
        cycle.end = cycle.start + cycle.frames
        if cycle.loop:
            poses = [cycle.pose(i / cycle.frames) for i in range(cycle.frames)]
            frames += poses + [poses[0], poses[0]]
        else:
            poses = [cycle.pose(i / cycle.frames) for i in range(cycle.frames + 1)]
            frames += poses + [poses[-1]]
    return frames


def world_pose(root, rot):
    """World rotation and joint position of every bone."""
    rots, pos = [], []
    for i, parent in enumerate(PARENTS):
        local = rot.get(BONES[i][0], IDENTITY)
        if parent < 0:
            rots.append(local)
            pos.append(root)
        else:
            offset = [JOINTS[i][k] - JOINTS[parent][k] for k in range(3)]
            rots.append(quat_mul(local, rots[parent]))
            pos.append(tuple(pos[parent][k] + quat_rotate(rots[parent], offset)[k] for k in range(3)))
    return rots, pos


def check_engine_skinning(mesh_list, frames, cycles):
    """Replays the engine's skinning on the stored (mirrored) data and compares it with the intended pose.

    AnimationImp::BuildAnimationMatrices: bone = local * parent with row vectors, skin = start^-1 * bone,
    model.cpp VBTransform: v * skin with x negated. Catches convention mistakes before the engine runs.
    """
    def mat(q, t):
        x, y, z, w = q
        return [[1 - 2 * (y * y + z * z), 2 * (x * y + w * z), 2 * (x * z - w * y), 0],
                [2 * (x * y - w * z), 1 - 2 * (x * x + z * z), 2 * (y * z + w * x), 0],
                [2 * (x * z + w * y), 2 * (y * z - w * x), 1 - 2 * (x * x + y * y), 0],
                [t[0], t[1], t[2], 1]]

    def mul(a, b):
        return [[sum(a[r][k] * b[k][c] for k in range(4)) for c in range(4)] for r in range(4)]

    def inverse_rigid(m):
        rt = [[m[c][r] for c in range(3)] for r in range(3)]
        t = [-sum(m[3][k] * rt[k][c] for k in range(3)) for c in range(3)]
        return [rt[0] + [0], rt[1] + [0], rt[2] + [0], t + [1]]

    def stored(q):
        return (q[0], -q[1], -q[2], q[3])

    def mirror(v):
        return (-v[0], v[1], v[2])

    def bones(root, rot):
        result = []
        for i, parent in enumerate(PARENTS):
            local = stored(rot.get(BONES[i][0], IDENTITY))
            if parent < 0:
                result.append(mat(local, mirror(root)))
            else:
                offset = mirror([JOINTS[i][k] - JOINTS[parent][k] for k in range(3)])
                result.append(mul(mat(local, offset), result[parent]))
        return result

    start = [inverse_rigid(m) for m in bones(*frames[0])]
    worst = 0.0
    for f in [0] + [c.start + c.frames * k // 3 for c in cycles for k in (1, 2)]:
        skin = [mul(start[i], m) for i, m in enumerate(bones(*frames[f]))]
        rots, pos = world_pose(*frames[f])
        for mesh in mesh_list:
            for p, _, _, b in mesh.vertices:
                v = mirror(p)
                engine = mirror([sum(v[k] * skin[b][k][c] for k in range(3)) + skin[b][3][c] for c in range(3)])
                local = [p[k] - JOINTS[b][k] for k in range(3)]
                intended = [pos[b][k] + quat_rotate(rots[b], local)[k] for k in range(3)]
                worst = max(worst, math.dist(engine, intended))
    assert worst < 1e-4, f"engine skinning differs from the intended pose by {worst}"

    # and the gaits do what they say: the left foot ahead at a quarter of the cycle
    for cycle in cycles:
        if cycle.stride:
            _, pos = world_pose(*frames[cycle.start + cycle.frames // 4])
            assert pos[BONE["knee_l"]][2] > 0.1 > -0.1 > pos[BONE["knee_r"]][2], f"{cycle.name}: left leg should lead"


def write_sailor(model_path, an_path, texture_prefix, cycles, labels=()):
    """Writes the skinned model and the .an with the given cycles (their start/end are set);
    labels are passed to write_gm, such as the bone locators a blade hangs on."""
    mesh_list = meshes(texture_prefix)
    frames = frame_list(cycles)
    check_engine_skinning(mesh_list, frames, cycles)
    write_gm(mesh_list, model_path, skinned=True, labels=labels)
    rotations = [[rot.get(name, IDENTITY) for _, rot in frames] for name, _, _ in BONES]
    joints = [JOINTS[0]] + [tuple(JOINTS[i][k] - JOINTS[p][k] for k in range(3))
                            for i, p in enumerate(PARENTS) if p >= 0]
    write_an(an_path, FPS, PARENTS, joints, [root for root, _ in frames], rotations)
