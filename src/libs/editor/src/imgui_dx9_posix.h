// Forced into the ImGui dx9 binding off Windows: it clears its structs with the Win32 ZeroMemory,
// which the d3d9 headers of dxvk-native do not define.
#pragma once
#include <cstring>
#ifndef ZeroMemory
#define ZeroMemory(dest, size) std::memset((dest), 0, (size))
#endif
