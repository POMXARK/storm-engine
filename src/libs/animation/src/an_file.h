#pragma once

// ANIMATION_HEADER
// int32_t parentIndex[nJoints]
// CVECTOR jointPos[nJoints]

// Frames:
// CVECTOR rootPos[nFrames]
// CVECTOR jointAngle[nJoints][nFrames]

#pragma pack(push, 1)

namespace ANFILE
{
// nJoints | MOVING_JOINTS: rootPos holds the positions of every bone, jointPos[nJoints][nFrames]
constexpr int32_t MOVING_JOINTS = 0x40000000;

struct HEADER
{
    int32_t nFrames;
    int32_t nJoints;
    float framesPerSec;
};
}; // namespace ANFILE

#pragma pack(pop)
