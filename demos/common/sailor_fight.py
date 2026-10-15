"""Fencing for the generated sailor: a guard, two cuts, block, parry, hits, death and the blade
locators, under the action names a location Character plays in fight mode (location/src/character.cpp).

The poses are keyframes of Euler angles per bone (sailor.euler) blended with smoothstep; the root
height is not keyed but found every frame, so that the lowest point of the body touches the ground:
the feet when standing, the back when lying dead.

The blade is held by the right hand: the "saber_hand" label on the forearm (elbow_r), its y axis
along the blade. With the arm hanging the blade points forward and down, so with the forearm raised
forward it points forward and up, as in a guard. "saber_belt" on the pelvis holds the sheathed blade
at the left hip, pointing down and back.
"""
import math

import sailor
from sailor import BONE, BONES, Cycle, JOINTS, euler, world_pose
from storm_formats import quat_rotate

# ---------------------------------------------------------------- blade locators
HAND = (-0.235, 0.83, 0.02)  # inside the right fist (the hand ellipsoid of sailor.meshes)
BLADE_IN_HAND = (0.0, -0.8, 1.0)  # forward and down while the arm hangs
BELT = (0.2, 0.9, 0.06)
BLADE_ON_BELT = (0.0, -1.0, -0.55)


def blade_labels():
    """write_gm labels: the hand and the belt locators BLADE moves the blade between."""
    return [("blade", "saber_hand", HAND, BLADE_IN_HAND, BONE["elbow_r"]),
            ("blade", "saber_belt", BELT, BLADE_ON_BELT, BONE["pelvis"])]


# ---------------------------------------------------------------- grounding
# points on the surface that can touch the ground, per bone, in the bind pose
CONTACTS = [("knee_l", (x, 0.0, z)) for x in (0.045, 0.155) for z in (-0.07, 0.16)] + \
           [("knee_r", (x, 0.0, z)) for x in (-0.045, -0.155) for z in (-0.07, 0.16)] + [
    ("pelvis", (0.0, 0.85, -0.11)), ("pelvis", (0.0, 0.85, 0.11)),
    ("chest", (0.0, 1.2, -0.1)), ("chest", (0.0, 1.38, -0.11)), ("chest", (0.0, 1.2, 0.1)),
    ("chest", (0.19, 1.3, 0.0)), ("chest", (-0.19, 1.3, 0.0)),
    ("neck", (0.0, 1.63, -0.115)), ("neck", (0.0, 1.76, 0.0)), ("neck", (0.0, 1.63, 0.115)),
    ("elbow_l", (0.235, 0.8, 0.01)), ("elbow_r", (-0.235, 0.8, 0.01)),
    ("knee_l", (0.1, 0.5, -0.06)), ("knee_r", (-0.1, 0.5, -0.06)),
    ("knee_l", (0.1, 0.5, 0.06)), ("knee_r", (-0.1, 0.5, 0.06)),
]


def lowest_point(root, rot):
    rots, pos = world_pose(root, rot)
    lowest = math.inf
    for name, point in CONTACTS:
        b = BONE[name]
        local = [point[k] - JOINTS[b][k] for k in range(3)]
        lowest = min(lowest, pos[b][1] + quat_rotate(rots[b], local)[1])
    return lowest


def grounded(offset, rot):
    """The root at offset (x, z) from the bind position, lowered or raised onto the ground."""
    root = (offset[0], JOINTS[0][1], offset[1])
    return (root[0], root[1] - lowest_point(root, rot), root[2]), rot


# ---------------------------------------------------------------- keyframes

def smoothstep(x):
    return x * x * (3 - 2 * x)


def blend(a, b, k):
    """Two keyed poses {bone: angles, "root": (x, z)} mixed by k; missing angles are 0."""
    result = {}
    for name in set(a) | set(b):
        va, vb = a.get(name, ()), b.get(name, ())
        n = max(len(va), len(vb))
        va, vb = tuple(va) + (0.0,) * (n - len(va)), tuple(vb) + (0.0,) * (n - len(vb))
        result[name] = tuple(x + (y - x) * k for x, y in zip(va, vb))
    return result


def to_pose(keyed):
    rot = {name: euler(*angles) for name, angles in keyed.items() if name != "root"}
    return grounded(keyed.get("root", (0.0, 0.0)), rot)


def keyframes(keys):
    """A pose function of t in 0..1 through [(t, keyed pose)], eased between neighbours."""
    def pose(t):
        for (t0, a), (t1, b) in zip(keys, keys[1:]):
            if t0 <= t <= t1:
                return to_pose(blend(a, b, smoothstep((t - t0) / (t1 - t0)) if t1 > t0 else 1.0))
        return to_pose(keys[-1][1])
    return pose


def merge(*poses):
    result = {}
    for p in poses:
        result.update(p)
    return result


# ---------------------------------------------------------------- poses
# angles (x, y, z[, twist]); x < 0 swings an arm or a leg forward, y > 0 turns towards the left
# (+x), z < 0 lifts the right arm sideways; a twist turns a forearm and the blade with it

REST = {"elbow_l": (-0.15,), "elbow_r": (-0.15,)}

GUARD_LEGS = {
    # right foot forward, the body turned to show the right shoulder, knees soft
    "pelvis": (0.0, 0.35, 0.0),
    "hip_r": (-0.45, 0.0, -0.08), "knee_r": (0.55,),
    "hip_l": (0.28, 0.0, 0.1), "knee_l": (0.25,),
}
GUARD_ARMS = {
    "chest": (0.08, 0.15, 0.0),
    "neck": (0.0, -0.45, 0.0),
    # the blade forward and up, the left hand raised for balance
    "shoulder_r": (-0.75, -0.4, -0.2), "elbow_r": (-0.7,),
    "shoulder_l": (0.25, 0.0, 0.4), "elbow_l": (-1.3,),
}
GUARD = merge(GUARD_LEGS, GUARD_ARMS)


def guard_with(**changes):
    return merge(GUARD, changes)


def fight_stand_pose(t):
    p = 2 * math.pi * t
    keyed = merge(GUARD, {"chest": (0.08 + 0.02 * math.sin(2 * p), 0.15, 0.0),
                          "shoulder_r": (-0.75 + 0.04 * math.sin(p), -0.4, -0.2)})
    return to_pose(keyed)


def fight_gait_pose(t, hip, knee, bounce):
    """Steps with the guard kept: the legs of sailor.gait_pose, the arms of the guard."""
    root, rot = sailor.gait_pose(t, hip=hip, knee=knee, arm=0.0, elbow=0.0, lean=0.1, bounce=bounce)
    keyed = merge(GUARD_ARMS, {"chest": (0.1, 0.45, 0.0), "neck": (-0.05, -0.45, 0.0)})
    for name, angles in keyed.items():
        rot[name] = euler(*angles)
    return root, rot


ATTACK_FAST = [  # a quick cut from above, the hit at the half
    (0.0, GUARD),
    (0.3, guard_with(shoulder_r=(-2.3, 0.0, -0.3), elbow_r=(-0.9,), chest=(-0.05, 0.3, 0.0))),
    (0.5, guard_with(shoulder_r=(-1.1, 0.25, -0.1), elbow_r=(-0.15,), chest=(0.22, -0.05, 0.0),
                     hip_r=(-0.75, 0.0, -0.08), knee_r=(0.75,), root=(0.0, 0.12))),
    (0.7, guard_with(shoulder_r=(-0.5, 0.35, 0.0), elbow_r=(-0.2,), chest=(0.2, -0.1, 0.0), root=(0.0, 0.08))),
    (1.0, GUARD),
]
ATTACK_FORCE = [  # a heavy chop with a lunge, a long swing back
    (0.0, GUARD),
    (0.4, guard_with(shoulder_r=(-2.9, 0.0, -0.4), elbow_r=(-1.3,), chest=(-0.15, 0.45, 0.0),
                     shoulder_l=(-0.5, 0.0, 0.6), elbow_l=(-0.4,), root=(0.0, -0.05))),
    (0.6, guard_with(shoulder_r=(-1.0, 0.2, 0.0), elbow_r=(-0.05,), chest=(0.35, -0.1, 0.0),
                     shoulder_l=(0.6, 0.0, 0.5), elbow_l=(-0.3,),
                     hip_r=(-0.95, 0.0, -0.08), knee_r=(0.95,), hip_l=(0.5, 0.0, 0.1), knee_l=(0.1,),
                     root=(0.0, 0.3))),
    (0.75, guard_with(shoulder_r=(-0.3, 0.3, 0.0), elbow_r=(-0.1,), chest=(0.35, -0.15, 0.0),
                      hip_r=(-0.95, 0.0, -0.08), knee_r=(0.95,), hip_l=(0.5, 0.0, 0.1), knee_l=(0.1,),
                      root=(0.0, 0.3))),
    (1.0, GUARD),
]
# the blade held across above the head
BLOCK_POSE = guard_with(shoulder_r=(-1.5, -0.4, -0.2), elbow_r=(-1.3, 0.0, 0.0, 1.0), chest=(0.0, 0.1, 0.0))


def block_pose(t):
    p = 2 * math.pi * t
    return to_pose(merge(BLOCK_POSE, {"chest": (0.015 * math.sin(p), 0.1, 0.0)}))


BLOCK_HIT = [  # the blow lands on the blade and pushes it back
    (0.0, BLOCK_POSE),
    (0.3, merge(BLOCK_POSE, {"shoulder_r": (-1.3, -0.3, -0.3), "chest": (-0.2, 0.15, 0.0), "root": (0.0, -0.08)})),
    (1.0, BLOCK_POSE),
]
PARRY = [  # a sweep across that knocks the enemy's blade aside
    (0.0, GUARD),
    (0.25, guard_with(shoulder_r=(-1.5, 0.6, -0.1), elbow_r=(-0.8, 0.0, 0.0, 1.2))),
    (0.55, guard_with(shoulder_r=(-1.6, -0.5, -0.3), elbow_r=(-0.8, 0.0, 0.0, -1.0), chest=(0.05, -0.1, 0.0))),
    (1.0, GUARD),
]
HIT = [  # struck: the body thrown back, the blade arm flung out
    (0.0, GUARD),
    (0.3, guard_with(chest=(-0.35, 0.3, 0.1), neck=(-0.3, -0.3, 0.0), shoulder_r=(-0.3, 0.0, -0.5),
                     elbow_r=(-0.4,), shoulder_l=(0.2, 0.0, 0.8), root=(0.0, -0.15))),
    (1.0, GUARD),
]
HIT_PARRY = [  # parried: thrown off balance for longer
    (0.0, GUARD),
    (0.35, guard_with(chest=(-0.3, -0.4, -0.1), neck=(-0.2, 0.0, 0.0), shoulder_r=(-0.6, -0.6, -0.9),
                      elbow_r=(-0.3,), shoulder_l=(0.3, 0.0, 0.7), root=(0.0, -0.25))),
    (0.6, guard_with(chest=(-0.15, -0.2, 0.0), shoulder_r=(-0.5, -0.3, -0.5), root=(0.0, -0.25))),
    (1.0, GUARD),
]
DRAW = [  # the right hand reaches to the left hip ("Blade to hand" at 40 %), then into the guard
    (0.0, REST),
    (0.4, merge(REST, {"shoulder_r": (-0.4, 0.0, 0.5), "elbow_r": (-0.6,), "chest": (0.1, 0.2, 0.0)})),
    (1.0, GUARD),
]
SHEATHE = [(1.0 - t, pose) for t, pose in reversed(DRAW)]  # "Blade to belt" at 60 %
DEATH = [  # the knees give way, then he falls on his back
    (0.0, GUARD),
    (0.3, {"hip_l": (-1.2, 0.0, 0.1), "hip_r": (-1.1, 0.0, -0.1), "knee_l": (1.8,), "knee_r": (1.9,),
           "chest": (0.45, 0.0, 0.0), "neck": (0.3,), "shoulder_r": (-0.3, 0.0, -0.3), "elbow_r": (-0.3,),
           "shoulder_l": (-0.2, 0.0, 0.3), "elbow_l": (-0.3,)}),
    (0.65, {"pelvis": (-0.9,), "hip_l": (-1.0, 0.0, 0.1), "hip_r": (-0.9, 0.0, -0.1), "knee_l": (1.2,),
            "knee_r": (1.4,), "chest": (-0.1,), "neck": (0.1,), "shoulder_r": (0.0, 0.0, -0.7),
            "shoulder_l": (0.0, 0.0, 0.7), "elbow_r": (-0.3,), "elbow_l": (-0.3,), "root": (0.0, -0.4)}),
    (1.0, {"pelvis": (-1.5,), "hip_l": (-0.15, 0.0, 0.12), "hip_r": (0.0, 0.0, -0.15), "knee_l": (0.35,),
           "knee_r": (0.1,), "neck": (-0.1, 0.5, 0.0), "shoulder_r": (0.2, 0.0, -1.1),
           "shoulder_l": (0.2, 0.0, 1.0), "elbow_r": (-0.4,), "elbow_l": (-0.25,), "root": (0.0, -0.75)}),
]

FIGHT_STAND = Cycle("fight stand", fight_stand_pose, 90)
FIGHT_WALK = Cycle("fight walk", lambda t: fight_gait_pose(t, hip=0.3, knee=0.6, bounce=0.02), 32, stride=1.0)
FIGHT_RUN = Cycle("fight run", lambda t: fight_gait_pose(t, hip=0.55, knee=1.1, bounce=0.04), 22, stride=2.0)
ATTACK_FAST_1 = Cycle("attack_fast_1", keyframes(ATTACK_FAST), 18, loop=False)
ATTACK_FORCE_1 = Cycle("attack_force_1", keyframes(ATTACK_FORCE), 27, loop=False)
BLOCK = Cycle("block", block_pose, 40)
BLOCK_HIT_1 = Cycle("block_hit", keyframes(BLOCK_HIT), 12, loop=False)
PARRY_1 = Cycle("parry_1", keyframes(PARRY), 18, loop=False)
HIT_1 = Cycle("hit_attack_1", keyframes(HIT), 15, loop=False)
HIT_PARRY_1 = Cycle("hit_parry", keyframes(HIT_PARRY), 24, loop=False)
NORMAL_TO_FIGHT = Cycle("Normal to fight", keyframes(DRAW), 15, loop=False)
FIGHT_TO_NORMAL = Cycle("Fight to normal", keyframes(SHEATHE), 15, loop=False)
DEATH_1 = Cycle("death_1", keyframes(DEATH), 45, loop=False)

CYCLES = [FIGHT_STAND, FIGHT_WALK, FIGHT_RUN, ATTACK_FAST_1, ATTACK_FORCE_1, BLOCK, BLOCK_HIT_1, PARRY_1,
          HIT_1, HIT_PARRY_1, NORMAL_TO_FIGHT, FIGHT_TO_NORMAL, DEATH_1]

# sections of the .ani besides start, end and loop: the events Character::Event reacts to
EVENTS = {
    "attack_fast_1": ['"Attack", 50%'],
    "attack_force_1": ['"Attack", 60%'],
    "parry_1": ['"Parry start", 10%', '"Parry end", 60%'],
    "Normal to fight": ['"Blade to hand", 40%'],
    "Fight to normal": ['"Blade to belt", 60%'],
}


def ani_sections():
    """(.ani section name, cycle, extra lines) for every fight action; the back steps play the
    forward ones reversed with a negative speed."""
    def speed(cycle, sign=1):
        return f'data = "Walk speed", "{sign * cycle.speed:.3f}"\n'

    sections = []
    for cycle in CYCLES:
        extra = "".join(f"event = {e}\n" for e in EVENTS.get(cycle.name, []))
        if cycle.stride:
            extra += speed(cycle)
        sections.append((cycle.name, cycle, extra))
        if cycle.stride:
            sections.append((cycle.name.replace("fight ", "fight back "), cycle,
                             "type = reverse\n" + speed(cycle, -1)))
    return sections


def check_blade(cycle, t, expect):
    """The blade direction in the world at t of a cycle: a sanity check of the angle conventions."""
    root, rot = cycle.pose(t)
    rots, _ = world_pose(root, rot)
    d = quat_rotate(rots[BONE["elbow_r"]], BLADE_IN_HAND)
    length = math.sqrt(sum(c * c for c in d))
    d = [c / length for c in d]
    assert expect(d), f"{cycle.name} at {t}: blade points {d}"


def self_check():
    # in the guard the blade points up and ahead, at the cut it comes down in front
    check_blade(FIGHT_STAND, 0.0, lambda d: d[1] > 0.3 and d[2] > 0.3)
    check_blade(ATTACK_FAST_1, 0.3, lambda d: d[2] < 0.0)
    for cycle in CYCLES:
        root, _ = cycle.pose(0.5)
        assert 0.3 < root[1] < 1.2, f"{cycle.name}: root at {root}"
    assert len(BONES) == 11
