
#include "UnrealSandboxTerrainPrivatePCH.h"
//#include "SandboxMobile.h"
#include "SandboxVoxeldata.h"
//#include "SandboxShared.h"

#include <cmath>

static const int edgeTable[256] = {
    0x0,   0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c, 0x80c, 0x905, 0xa0f, 0xb06, 0xc0a, 0xd03, 0xe09, 0xf00,
    0x190, 0x99,  0x393, 0x29a, 0x596, 0x49f, 0x795, 0x69c, 0x99c, 0x895, 0xb9f, 0xa96, 0xd9a, 0xc93, 0xf99, 0xe90,
    0x230, 0x339, 0x33,  0x13a, 0x636, 0x73f, 0x435, 0x53c, 0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30,
    0x3a0, 0x2a9, 0x1a3, 0xaa,  0x7a6, 0x6af, 0x5a5, 0x4ac, 0xbac, 0xaa5, 0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0,
    0x460, 0x569, 0x663, 0x76a, 0x66,  0x16f, 0x265, 0x36c, 0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a, 0x963, 0xa69, 0xb60,
    0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0xff,  0x3f5, 0x2fc, 0xdfc, 0xcf5, 0xfff, 0xef6, 0x9fa, 0x8f3, 0xbf9, 0xaf0,
    0x650, 0x759, 0x453, 0x55a, 0x256, 0x35f, 0x55,  0x15c, 0xe5c, 0xf55, 0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950,
    0x7c0, 0x6c9, 0x5c3, 0x4ca, 0x3c6, 0x2cf, 0x1c5, 0xcc,  0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca, 0xac3, 0x9c9, 0x8c0,
    0x8c0, 0x9c9, 0xac3, 0xbca, 0xcc6, 0xdcf, 0xec5, 0xfcc, 0xcc,  0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0,
    0x950, 0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c, 0x15c, 0x55,  0x35f, 0x256, 0x55a, 0x453, 0x759, 0x650,
    0xaf0, 0xbf9, 0x8f3, 0x9fa, 0xef6, 0xfff, 0xcf5, 0xdfc, 0x2fc, 0x3f5, 0xff,  0x1f6, 0x6fa, 0x7f3, 0x4f9, 0x5f0,
    0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65, 0xc6c, 0x36c, 0x265, 0x16f, 0x66,  0x76a, 0x663, 0x569, 0x460,
    0xca0, 0xda9, 0xea3, 0xfaa, 0x8a6, 0x9af, 0xaa5, 0xbac, 0x4ac, 0x5a5, 0x6af, 0x7a6, 0xaa,  0x1a3, 0x2a9, 0x3a0,
    0xd30, 0xc39, 0xf33, 0xe3a, 0x936, 0x83f, 0xb35, 0xa3c, 0x53c, 0x435, 0x73f, 0x636, 0x13a, 0x33,  0x339, 0x230,
    0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895, 0x99c, 0x69c, 0x795, 0x49f, 0x596, 0x29a, 0x393, 0x99,  0x190,
    0xf00, 0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c, 0x70c, 0x605, 0x50f, 0x406, 0x30a, 0x203, 0x109, 0x0
};

static const int triTable[256][16] = { { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 0, 1, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 1, 8, 3, 9, 8, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 0, 8, 3, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 9, 2, 10, 0, 2, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 2, 8, 3, 2, 10, 8, 10, 9, 8, -1, -1, -1, -1, -1, -1, -1 },
                                       { 3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 0, 11, 2, 8, 11, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 1, 9, 0, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 1, 11, 2, 1, 9, 11, 9, 8, 11, -1, -1, -1, -1, -1, -1, -1 },
                                       { 3, 10, 1, 11, 10, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 0, 10, 1, 0, 8, 10, 8, 11, 10, -1, -1, -1, -1, -1, -1, -1 },
                                       { 3, 9, 0, 3, 11, 9, 11, 10, 9, -1, -1, -1, -1, -1, -1, -1 },
                                       { 9, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 4, 3, 0, 7, 3, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 0, 1, 9, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 4, 1, 9, 4, 7, 1, 7, 3, 1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 1, 2, 10, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 3, 4, 7, 3, 0, 4, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1 },
                                       { 9, 2, 10, 9, 0, 2, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1 },
                                       { 2, 10, 9, 2, 9, 7, 2, 7, 3, 7, 9, 4, -1, -1, -1, -1 },
                                       { 8, 4, 7, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 11, 4, 7, 11, 2, 4, 2, 0, 4, -1, -1, -1, -1, -1, -1, -1 },
                                       { 9, 0, 1, 8, 4, 7, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1 },
                                       { 4, 7, 11, 9, 4, 11, 9, 11, 2, 9, 2, 1, -1, -1, -1, -1 },
                                       { 3, 10, 1, 3, 11, 10, 7, 8, 4, -1, -1, -1, -1, -1, -1, -1 },
                                       { 1, 11, 10, 1, 4, 11, 1, 0, 4, 7, 11, 4, -1, -1, -1, -1 },
                                       { 4, 7, 8, 9, 0, 11, 9, 11, 10, 11, 0, 3, -1, -1, -1, -1 },
                                       { 4, 7, 11, 4, 11, 9, 9, 11, 10, -1, -1, -1, -1, -1, -1, -1 },
                                       { 9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 9, 5, 4, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 0, 5, 4, 1, 5, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 8, 5, 4, 8, 3, 5, 3, 1, 5, -1, -1, -1, -1, -1, -1, -1 },
                                       { 1, 2, 10, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 3, 0, 8, 1, 2, 10, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1 },
                                       { 5, 2, 10, 5, 4, 2, 4, 0, 2, -1, -1, -1, -1, -1, -1, -1 },
                                       { 2, 10, 5, 3, 2, 5, 3, 5, 4, 3, 4, 8, -1, -1, -1, -1 },
                                       { 9, 5, 4, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 0, 11, 2, 0, 8, 11, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1 },
                                       { 0, 5, 4, 0, 1, 5, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1 },
                                       { 2, 1, 5, 2, 5, 8, 2, 8, 11, 4, 8, 5, -1, -1, -1, -1 },
                                       { 10, 3, 11, 10, 1, 3, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1 },
                                       { 4, 9, 5, 0, 8, 1, 8, 10, 1, 8, 11, 10, -1, -1, -1, -1 },
                                       { 5, 4, 0, 5, 0, 11, 5, 11, 10, 11, 0, 3, -1, -1, -1, -1 },
                                       { 5, 4, 8, 5, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1 },
                                       { 9, 7, 8, 5, 7, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 9, 3, 0, 9, 5, 3, 5, 7, 3, -1, -1, -1, -1, -1, -1, -1 },
                                       { 0, 7, 8, 0, 1, 7, 1, 5, 7, -1, -1, -1, -1, -1, -1, -1 },
                                       { 1, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 9, 7, 8, 9, 5, 7, 10, 1, 2, -1, -1, -1, -1, -1, -1, -1 },
                                       { 10, 1, 2, 9, 5, 0, 5, 3, 0, 5, 7, 3, -1, -1, -1, -1 },
                                       { 8, 0, 2, 8, 2, 5, 8, 5, 7, 10, 5, 2, -1, -1, -1, -1 },
                                       { 2, 10, 5, 2, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1 },
                                       { 7, 9, 5, 7, 8, 9, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1 },
                                       { 9, 5, 7, 9, 7, 2, 9, 2, 0, 2, 7, 11, -1, -1, -1, -1 },
                                       { 2, 3, 11, 0, 1, 8, 1, 7, 8, 1, 5, 7, -1, -1, -1, -1 },
                                       { 11, 2, 1, 11, 1, 7, 7, 1, 5, -1, -1, -1, -1, -1, -1, -1 },
                                       { 9, 5, 8, 8, 5, 7, 10, 1, 3, 10, 3, 11, -1, -1, -1, -1 },
                                       { 5, 7, 0, 5, 0, 9, 7, 11, 0, 1, 0, 10, 11, 10, 0, -1 },
                                       { 11, 10, 0, 11, 0, 3, 10, 5, 0, 8, 0, 7, 5, 7, 0, -1 },
                                       { 11, 10, 5, 7, 11, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 0, 8, 3, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 9, 0, 1, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 1, 8, 3, 1, 9, 8, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1 },
                                       { 1, 6, 5, 2, 6, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 1, 6, 5, 1, 2, 6, 3, 0, 8, -1, -1, -1, -1, -1, -1, -1 },
                                       { 9, 6, 5, 9, 0, 6, 0, 2, 6, -1, -1, -1, -1, -1, -1, -1 },
                                       { 5, 9, 8, 5, 8, 2, 5, 2, 6, 3, 2, 8, -1, -1, -1, -1 },
                                       { 2, 3, 11, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 11, 0, 8, 11, 2, 0, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1 },
                                       { 0, 1, 9, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1 },
                                       { 5, 10, 6, 1, 9, 2, 9, 11, 2, 9, 8, 11, -1, -1, -1, -1 },
                                       { 6, 3, 11, 6, 5, 3, 5, 1, 3, -1, -1, -1, -1, -1, -1, -1 },
                                       { 0, 8, 11, 0, 11, 5, 0, 5, 1, 5, 11, 6, -1, -1, -1, -1 },
                                       { 3, 11, 6, 0, 3, 6, 0, 6, 5, 0, 5, 9, -1, -1, -1, -1 },
                                       { 6, 5, 9, 6, 9, 11, 11, 9, 8, -1, -1, -1, -1, -1, -1, -1 },
                                       { 5, 10, 6, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 4, 3, 0, 4, 7, 3, 6, 5, 10, -1, -1, -1, -1, -1, -1, -1 },
                                       { 1, 9, 0, 5, 10, 6, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1 },
                                       { 10, 6, 5, 1, 9, 7, 1, 7, 3, 7, 9, 4, -1, -1, -1, -1 },
                                       { 6, 1, 2, 6, 5, 1, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1 },
                                       { 1, 2, 5, 5, 2, 6, 3, 0, 4, 3, 4, 7, -1, -1, -1, -1 },
                                       { 8, 4, 7, 9, 0, 5, 0, 6, 5, 0, 2, 6, -1, -1, -1, -1 },
                                       { 7, 3, 9, 7, 9, 4, 3, 2, 9, 5, 9, 6, 2, 6, 9, -1 },
                                       { 3, 11, 2, 7, 8, 4, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1 },
                                       { 5, 10, 6, 4, 7, 2, 4, 2, 0, 2, 7, 11, -1, -1, -1, -1 },
                                       { 0, 1, 9, 4, 7, 8, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1 },
                                       { 9, 2, 1, 9, 11, 2, 9, 4, 11, 7, 11, 4, 5, 10, 6, -1 },
                                       { 8, 4, 7, 3, 11, 5, 3, 5, 1, 5, 11, 6, -1, -1, -1, -1 },
                                       { 5, 1, 11, 5, 11, 6, 1, 0, 11, 7, 11, 4, 0, 4, 11, -1 },
                                       { 0, 5, 9, 0, 6, 5, 0, 3, 6, 11, 6, 3, 8, 4, 7, -1 },
                                       { 6, 5, 9, 6, 9, 11, 4, 7, 9, 7, 11, 9, -1, -1, -1, -1 },
                                       { 10, 4, 9, 6, 4, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 4, 10, 6, 4, 9, 10, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1 },
                                       { 10, 0, 1, 10, 6, 0, 6, 4, 0, -1, -1, -1, -1, -1, -1, -1 },
                                       { 8, 3, 1, 8, 1, 6, 8, 6, 4, 6, 1, 10, -1, -1, -1, -1 },
                                       { 1, 4, 9, 1, 2, 4, 2, 6, 4, -1, -1, -1, -1, -1, -1, -1 },
                                       { 3, 0, 8, 1, 2, 9, 2, 4, 9, 2, 6, 4, -1, -1, -1, -1 },
                                       { 0, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 8, 3, 2, 8, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1 },
                                       { 10, 4, 9, 10, 6, 4, 11, 2, 3, -1, -1, -1, -1, -1, -1, -1 },
                                       { 0, 8, 2, 2, 8, 11, 4, 9, 10, 4, 10, 6, -1, -1, -1, -1 },
                                       { 3, 11, 2, 0, 1, 6, 0, 6, 4, 6, 1, 10, -1, -1, -1, -1 },
                                       { 6, 4, 1, 6, 1, 10, 4, 8, 1, 2, 1, 11, 8, 11, 1, -1 },
                                       { 9, 6, 4, 9, 3, 6, 9, 1, 3, 11, 6, 3, -1, -1, -1, -1 },
                                       { 8, 11, 1, 8, 1, 0, 11, 6, 1, 9, 1, 4, 6, 4, 1, -1 },
                                       { 3, 11, 6, 3, 6, 0, 0, 6, 4, -1, -1, -1, -1, -1, -1, -1 },
                                       { 6, 4, 8, 11, 6, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 7, 10, 6, 7, 8, 10, 8, 9, 10, -1, -1, -1, -1, -1, -1, -1 },
                                       { 0, 7, 3, 0, 10, 7, 0, 9, 10, 6, 7, 10, -1, -1, -1, -1 },
                                       { 10, 6, 7, 1, 10, 7, 1, 7, 8, 1, 8, 0, -1, -1, -1, -1 },
                                       { 10, 6, 7, 10, 7, 1, 1, 7, 3, -1, -1, -1, -1, -1, -1, -1 },
                                       { 1, 2, 6, 1, 6, 8, 1, 8, 9, 8, 6, 7, -1, -1, -1, -1 },
                                       { 2, 6, 9, 2, 9, 1, 6, 7, 9, 0, 9, 3, 7, 3, 9, -1 },
                                       { 7, 8, 0, 7, 0, 6, 6, 0, 2, -1, -1, -1, -1, -1, -1, -1 },
                                       { 7, 3, 2, 6, 7, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 2, 3, 11, 10, 6, 8, 10, 8, 9, 8, 6, 7, -1, -1, -1, -1 },
                                       { 2, 0, 7, 2, 7, 11, 0, 9, 7, 6, 7, 10, 9, 10, 7, -1 },
                                       { 1, 8, 0, 1, 7, 8, 1, 10, 7, 6, 7, 10, 2, 3, 11, -1 },
                                       { 11, 2, 1, 11, 1, 7, 10, 6, 1, 6, 7, 1, -1, -1, -1, -1 },
                                       { 8, 9, 6, 8, 6, 7, 9, 1, 6, 11, 6, 3, 1, 3, 6, -1 },
                                       { 0, 9, 1, 11, 6, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 7, 8, 0, 7, 0, 6, 3, 11, 0, 11, 6, 0, -1, -1, -1, -1 },
                                       { 7, 11, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 3, 0, 8, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 0, 1, 9, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 8, 1, 9, 8, 3, 1, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1 },
                                       { 10, 1, 2, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 1, 2, 10, 3, 0, 8, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1 },
                                       { 2, 9, 0, 2, 10, 9, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1 },
                                       { 6, 11, 7, 2, 10, 3, 10, 8, 3, 10, 9, 8, -1, -1, -1, -1 },
                                       { 7, 2, 3, 6, 2, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 7, 0, 8, 7, 6, 0, 6, 2, 0, -1, -1, -1, -1, -1, -1, -1 },
                                       { 2, 7, 6, 2, 3, 7, 0, 1, 9, -1, -1, -1, -1, -1, -1, -1 },
                                       { 1, 6, 2, 1, 8, 6, 1, 9, 8, 8, 7, 6, -1, -1, -1, -1 },
                                       { 10, 7, 6, 10, 1, 7, 1, 3, 7, -1, -1, -1, -1, -1, -1, -1 },
                                       { 10, 7, 6, 1, 7, 10, 1, 8, 7, 1, 0, 8, -1, -1, -1, -1 },
                                       { 0, 3, 7, 0, 7, 10, 0, 10, 9, 6, 10, 7, -1, -1, -1, -1 },
                                       { 7, 6, 10, 7, 10, 8, 8, 10, 9, -1, -1, -1, -1, -1, -1, -1 },
                                       { 6, 8, 4, 11, 8, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 3, 6, 11, 3, 0, 6, 0, 4, 6, -1, -1, -1, -1, -1, -1, -1 },
                                       { 8, 6, 11, 8, 4, 6, 9, 0, 1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 9, 4, 6, 9, 6, 3, 9, 3, 1, 11, 3, 6, -1, -1, -1, -1 },
                                       { 6, 8, 4, 6, 11, 8, 2, 10, 1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 1, 2, 10, 3, 0, 11, 0, 6, 11, 0, 4, 6, -1, -1, -1, -1 },
                                       { 4, 11, 8, 4, 6, 11, 0, 2, 9, 2, 10, 9, -1, -1, -1, -1 },
                                       { 10, 9, 3, 10, 3, 2, 9, 4, 3, 11, 3, 6, 4, 6, 3, -1 },
                                       { 8, 2, 3, 8, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1 },
                                       { 0, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 1, 9, 0, 2, 3, 4, 2, 4, 6, 4, 3, 8, -1, -1, -1, -1 },
                                       { 1, 9, 4, 1, 4, 2, 2, 4, 6, -1, -1, -1, -1, -1, -1, -1 },
                                       { 8, 1, 3, 8, 6, 1, 8, 4, 6, 6, 10, 1, -1, -1, -1, -1 },
                                       { 10, 1, 0, 10, 0, 6, 6, 0, 4, -1, -1, -1, -1, -1, -1, -1 },
                                       { 4, 6, 3, 4, 3, 8, 6, 10, 3, 0, 3, 9, 10, 9, 3, -1 },
                                       { 10, 9, 4, 6, 10, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 4, 9, 5, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 0, 8, 3, 4, 9, 5, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1 },
                                       { 5, 0, 1, 5, 4, 0, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1 },
                                       { 11, 7, 6, 8, 3, 4, 3, 5, 4, 3, 1, 5, -1, -1, -1, -1 },
                                       { 9, 5, 4, 10, 1, 2, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1 },
                                       { 6, 11, 7, 1, 2, 10, 0, 8, 3, 4, 9, 5, -1, -1, -1, -1 },
                                       { 7, 6, 11, 5, 4, 10, 4, 2, 10, 4, 0, 2, -1, -1, -1, -1 },
                                       { 3, 4, 8, 3, 5, 4, 3, 2, 5, 10, 5, 2, 11, 7, 6, -1 },
                                       { 7, 2, 3, 7, 6, 2, 5, 4, 9, -1, -1, -1, -1, -1, -1, -1 },
                                       { 9, 5, 4, 0, 8, 6, 0, 6, 2, 6, 8, 7, -1, -1, -1, -1 },
                                       { 3, 6, 2, 3, 7, 6, 1, 5, 0, 5, 4, 0, -1, -1, -1, -1 },
                                       { 6, 2, 8, 6, 8, 7, 2, 1, 8, 4, 8, 5, 1, 5, 8, -1 },
                                       { 9, 5, 4, 10, 1, 6, 1, 7, 6, 1, 3, 7, -1, -1, -1, -1 },
                                       { 1, 6, 10, 1, 7, 6, 1, 0, 7, 8, 7, 0, 9, 5, 4, -1 },
                                       { 4, 0, 10, 4, 10, 5, 0, 3, 10, 6, 10, 7, 3, 7, 10, -1 },
                                       { 7, 6, 10, 7, 10, 8, 5, 4, 10, 4, 8, 10, -1, -1, -1, -1 },
                                       { 6, 9, 5, 6, 11, 9, 11, 8, 9, -1, -1, -1, -1, -1, -1, -1 },
                                       { 3, 6, 11, 0, 6, 3, 0, 5, 6, 0, 9, 5, -1, -1, -1, -1 },
                                       { 0, 11, 8, 0, 5, 11, 0, 1, 5, 5, 6, 11, -1, -1, -1, -1 },
                                       { 6, 11, 3, 6, 3, 5, 5, 3, 1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 1, 2, 10, 9, 5, 11, 9, 11, 8, 11, 5, 6, -1, -1, -1, -1 },
                                       { 0, 11, 3, 0, 6, 11, 0, 9, 6, 5, 6, 9, 1, 2, 10, -1 },
                                       { 11, 8, 5, 11, 5, 6, 8, 0, 5, 10, 5, 2, 0, 2, 5, -1 },
                                       { 6, 11, 3, 6, 3, 5, 2, 10, 3, 10, 5, 3, -1, -1, -1, -1 },
                                       { 5, 8, 9, 5, 2, 8, 5, 6, 2, 3, 8, 2, -1, -1, -1, -1 },
                                       { 9, 5, 6, 9, 6, 0, 0, 6, 2, -1, -1, -1, -1, -1, -1, -1 },
                                       { 1, 5, 8, 1, 8, 0, 5, 6, 8, 3, 8, 2, 6, 2, 8, -1 },
                                       { 1, 5, 6, 2, 1, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 1, 3, 6, 1, 6, 10, 3, 8, 6, 5, 6, 9, 8, 9, 6, -1 },
                                       { 10, 1, 0, 10, 0, 6, 9, 5, 0, 5, 6, 0, -1, -1, -1, -1 },
                                       { 0, 3, 8, 5, 6, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 10, 5, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 11, 5, 10, 7, 5, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 11, 5, 10, 11, 7, 5, 8, 3, 0, -1, -1, -1, -1, -1, -1, -1 },
                                       { 5, 11, 7, 5, 10, 11, 1, 9, 0, -1, -1, -1, -1, -1, -1, -1 },
                                       { 10, 7, 5, 10, 11, 7, 9, 8, 1, 8, 3, 1, -1, -1, -1, -1 },
                                       { 11, 1, 2, 11, 7, 1, 7, 5, 1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 0, 8, 3, 1, 2, 7, 1, 7, 5, 7, 2, 11, -1, -1, -1, -1 },
                                       { 9, 7, 5, 9, 2, 7, 9, 0, 2, 2, 11, 7, -1, -1, -1, -1 },
                                       { 7, 5, 2, 7, 2, 11, 5, 9, 2, 3, 2, 8, 9, 8, 2, -1 },
                                       { 2, 5, 10, 2, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1 },
                                       { 8, 2, 0, 8, 5, 2, 8, 7, 5, 10, 2, 5, -1, -1, -1, -1 },
                                       { 9, 0, 1, 5, 10, 3, 5, 3, 7, 3, 10, 2, -1, -1, -1, -1 },
                                       { 9, 8, 2, 9, 2, 1, 8, 7, 2, 10, 2, 5, 7, 5, 2, -1 },
                                       { 1, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 0, 8, 7, 0, 7, 1, 1, 7, 5, -1, -1, -1, -1, -1, -1, -1 },
                                       { 9, 0, 3, 9, 3, 5, 5, 3, 7, -1, -1, -1, -1, -1, -1, -1 },
                                       { 9, 8, 7, 5, 9, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 5, 8, 4, 5, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1 },
                                       { 5, 0, 4, 5, 11, 0, 5, 10, 11, 11, 3, 0, -1, -1, -1, -1 },
                                       { 0, 1, 9, 8, 4, 10, 8, 10, 11, 10, 4, 5, -1, -1, -1, -1 },
                                       { 10, 11, 4, 10, 4, 5, 11, 3, 4, 9, 4, 1, 3, 1, 4, -1 },
                                       { 2, 5, 1, 2, 8, 5, 2, 11, 8, 4, 5, 8, -1, -1, -1, -1 },
                                       { 0, 4, 11, 0, 11, 3, 4, 5, 11, 2, 11, 1, 5, 1, 11, -1 },
                                       { 0, 2, 5, 0, 5, 9, 2, 11, 5, 4, 5, 8, 11, 8, 5, -1 },
                                       { 9, 4, 5, 2, 11, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 2, 5, 10, 3, 5, 2, 3, 4, 5, 3, 8, 4, -1, -1, -1, -1 },
                                       { 5, 10, 2, 5, 2, 4, 4, 2, 0, -1, -1, -1, -1, -1, -1, -1 },
                                       { 3, 10, 2, 3, 5, 10, 3, 8, 5, 4, 5, 8, 0, 1, 9, -1 },
                                       { 5, 10, 2, 5, 2, 4, 1, 9, 2, 9, 4, 2, -1, -1, -1, -1 },
                                       { 8, 4, 5, 8, 5, 3, 3, 5, 1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 0, 4, 5, 1, 0, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 8, 4, 5, 8, 5, 3, 9, 0, 5, 0, 3, 5, -1, -1, -1, -1 },
                                       { 9, 4, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 4, 11, 7, 4, 9, 11, 9, 10, 11, -1, -1, -1, -1, -1, -1, -1 },
                                       { 0, 8, 3, 4, 9, 7, 9, 11, 7, 9, 10, 11, -1, -1, -1, -1 },
                                       { 1, 10, 11, 1, 11, 4, 1, 4, 0, 7, 4, 11, -1, -1, -1, -1 },
                                       { 3, 1, 4, 3, 4, 8, 1, 10, 4, 7, 4, 11, 10, 11, 4, -1 },
                                       { 4, 11, 7, 9, 11, 4, 9, 2, 11, 9, 1, 2, -1, -1, -1, -1 },
                                       { 9, 7, 4, 9, 11, 7, 9, 1, 11, 2, 11, 1, 0, 8, 3, -1 },
                                       { 11, 7, 4, 11, 4, 2, 2, 4, 0, -1, -1, -1, -1, -1, -1, -1 },
                                       { 11, 7, 4, 11, 4, 2, 8, 3, 4, 3, 2, 4, -1, -1, -1, -1 },
                                       { 2, 9, 10, 2, 7, 9, 2, 3, 7, 7, 4, 9, -1, -1, -1, -1 },
                                       { 9, 10, 7, 9, 7, 4, 10, 2, 7, 8, 7, 0, 2, 0, 7, -1 },
                                       { 3, 7, 10, 3, 10, 2, 7, 4, 10, 1, 10, 0, 4, 0, 10, -1 },
                                       { 1, 10, 2, 8, 7, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 4, 9, 1, 4, 1, 7, 7, 1, 3, -1, -1, -1, -1, -1, -1, -1 },
                                       { 4, 9, 1, 4, 1, 7, 0, 8, 1, 8, 7, 1, -1, -1, -1, -1 },
                                       { 4, 0, 3, 7, 4, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 4, 8, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 9, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 3, 0, 9, 3, 9, 11, 11, 9, 10, -1, -1, -1, -1, -1, -1, -1 },
                                       { 0, 1, 10, 0, 10, 8, 8, 10, 11, -1, -1, -1, -1, -1, -1, -1 },
                                       { 3, 1, 10, 11, 3, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 1, 2, 11, 1, 11, 9, 9, 11, 8, -1, -1, -1, -1, -1, -1, -1 },
                                       { 3, 0, 9, 3, 9, 11, 1, 2, 9, 2, 11, 9, -1, -1, -1, -1 },
                                       { 0, 2, 11, 8, 0, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 3, 2, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 2, 3, 8, 2, 8, 10, 10, 8, 9, -1, -1, -1, -1, -1, -1, -1 },
                                       { 9, 10, 2, 0, 9, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 2, 3, 8, 2, 8, 10, 0, 1, 8, 1, 10, 8, -1, -1, -1, -1 },
                                       { 1, 10, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 1, 3, 8, 9, 1, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 0, 9, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { 0, 3, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
                                       { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 } };


//====================================================================================
// Voxel data impl
//====================================================================================

    VoxelData::VoxelData(int num, float size){
       // int s = num*num*num;

        density_data = NULL;
		density_state = VoxelDataFillState::ZERO;

		material_data = NULL;

		voxel_num = num;
        volume_size = size;
    }

    VoxelData::~VoxelData(){
        delete[] density_data;
		delete[] material_data;
    }

	void VoxelData::initializeDensity() {
		int s = voxel_num * voxel_num * voxel_num;
		density_data = new unsigned char[s];
		for (auto x = 0; x < voxel_num; x++) {
			for (auto y = 0; y < voxel_num; y++) {
				for (auto z = 0; z < voxel_num; z++) {
					if (density_state == VoxelDataFillState::ALL) {
						setDensity(x, y, z, 1);
					}

					if (density_state == VoxelDataFillState::ZERO) {
						setDensity(x, y, z, 0);
					}
				}
			}
		}
	}

	void VoxelData::initializeMaterial() {
		int s = voxel_num * voxel_num * voxel_num;
		material_data = new unsigned char[s];
		for (auto x = 0; x < voxel_num; x++) {
			for (auto y = 0; y < voxel_num; y++) {
				for (auto z = 0; z < voxel_num; z++) {
					setMaterial(x, y, z, base_fill_mat);
				}
			}
		}
	}

    void VoxelData::setDensity(int x, int y, int z, float density){
		if (density_data == NULL) {
			if (density_state == VoxelDataFillState::ZERO && density == 0){
				return;
			}

			if (density_state == VoxelDataFillState::ALL && density == 1) {
				return;
			}

			initializeDensity();
			density_state = VoxelDataFillState::MIX;
		}

        if(x < voxel_num && y < voxel_num && z < voxel_num){
            int index = x * voxel_num * voxel_num + y * voxel_num + z;	

			if (density < 0) density = 0;
			if (density > 1) density = 1;

			unsigned char d = 255 * density;

			density_data[index] = d;
        }
    }

    float VoxelData::getDensity(int x, int y, int z) const {
		if (density_data == NULL) {
			if (density_state == VoxelDataFillState::ALL) {
				return 1;
			}

			return 0;
		}

        if(x < voxel_num && y < voxel_num && z < voxel_num){
			int index = x * voxel_num * voxel_num + y * voxel_num + z;

			float d = (float)density_data[index] / 255.0f;
            return d;
        } else {
            return 0;
        }
    }

	void VoxelData::setMaterial(int x, int y, int z, int material) {
		if (material_data == NULL) {
			initializeMaterial();
		}

		if (x < voxel_num && y < voxel_num && z < voxel_num) {
			int index = x * voxel_num * voxel_num + y * voxel_num + z;
			material_data[index] = material;
		}
	}

	int VoxelData::getMaterial(int x, int y, int z) const {
		if (material_data == NULL) {
			return base_fill_mat;
		}

		if (x < voxel_num && y < voxel_num && z < voxel_num) {
			int index = x * voxel_num * voxel_num + y * voxel_num + z;
			return material_data[index];
		} else {
			return 0;
		}
	}

	FVector VoxelData::voxelIndexToVector(int x, int y, int z) const {
		float step = size() / (num() - 1);
		float s = -size() / 2;
		FVector v(s, s, s);
		FVector a(x * step, y * step, z * step);
		v = v + a;
		return v;
	}

	void VoxelData::setOrigin(FVector o) {
		origin = o;
	}

	FVector VoxelData::getOrigin() const {
		return origin;
	}

    float VoxelData::size() const {
        return volume_size;
    }
    
    int VoxelData::num() const {
        return voxel_num;
    }

	VoxelPoint VoxelData::getVoxelPoint(int x, int y, int z) const {
		VoxelPoint vp;
		int index = x * voxel_num * voxel_num + y * voxel_num + z;

		vp.material = base_fill_mat;
		vp.density = 0;

		if (density_data != NULL) {
			vp.density = density_data[index];
		}

		if (material_data != NULL) {
			vp.material = material_data[index];
		}

		return vp;
	}

	void VoxelData::setVoxelPoint(int x, int y, int z, unsigned char density, unsigned char material) {
		if (density_data == NULL) {
			initializeDensity();
			density_state = VoxelDataFillState::MIX;
		}

		if (material_data == NULL) {
			initializeMaterial();
		}

		int index = x * voxel_num * voxel_num + y * voxel_num + z;
		material_data[index] = material;
		density_data[index] = density;
	}

	void VoxelData::setVoxelPointDensity(int x, int y, int z, unsigned char density) {
		if (density_data == NULL) {
			initializeDensity();
			density_state = VoxelDataFillState::MIX;
		}

		int index = x * voxel_num * voxel_num + y * voxel_num + z;
		density_data[index] = density;
	}

	void VoxelData::setVoxelPointMaterial(int x, int y, int z, unsigned char material) {
		if (material_data == NULL) {
			initializeMaterial();
		}

		int index = x * voxel_num * voxel_num + y * voxel_num + z;
		material_data[index] = material;
	}

	void VoxelData::deinitializeDensity(VoxelDataFillState state) {
		if (state == VoxelDataFillState::MIX) {
			return;
		}

		density_state = state;
		if (density_data != NULL) {
			delete density_data;
		}

		density_data = NULL;
	}

	void VoxelData::deinitializeMaterial(unsigned char base_mat) {
		base_fill_mat = base_mat;

		if (material_data != NULL) {
			delete material_data;
		}

		material_data = NULL;
	}

	VoxelDataFillState VoxelData::getDensityFillState()	const {
		return density_state;
	}


	//====================================================================================
	
static FORCEINLINE FVector clcNormal(FVector &p1, FVector &p2, FVector &p3) {
    float A = p1.Y * (p2.Z - p3.Z) + p2.Y * (p3.Z - p1.Z) + p3.Y * (p1.Z - p2.Z);
    float B = p1.Z * (p2.X - p3.X) + p2.Z * (p3.X - p1.X) + p3.Z * (p1.X - p2.X);
    float C = p1.X * (p2.Y - p3.Y) + p2.X * (p3.Y - p1.Y) + p3.X * (p1.Y - p2.Y);
    // float D = -(p1.x * (p2.y * p3.z - p3.y * p2.z) + p2.x * (p3.y * p1.z - p1.y * p3.z) + p3.x * (p1.y * p2.z - p2.y
    // * p1.z));

    FVector n(A, B, C);
    n.Normalize();
    return n;
}

class VoxelMeshProcessor {
public:
	int ntriang = 0;
	int vertex_index = 0;

	MeshDataElement &mesh_data;
	const VoxelData &voxel_data;
	const VoxelDataParam &voxel_data_param;

	VoxelMeshProcessor(MeshDataElement &a, const VoxelData &b, const VoxelDataParam &c) : mesh_data(a), voxel_data(b), voxel_data_param(c) {}
    
private:
	double isolevel = 0.5f;

	struct Point {
		float density;
		int material_id;
	};

	struct TmpPoint {
		FVector v;
		int mat_id;
		float mat_weight=0;
	};

	FORCEINLINE Point getVoxelpoint(int x, int y, int z) {
		Point vp;
		vp.density = getDensity(x, y, z);
		vp.material_id = getMaterial(x, y, z);
		return vp;
	}

	FORCEINLINE float getDensity(int x, int y, int z) {
		int step = voxel_data_param.lod;
		if (voxel_data_param.z_cut) {
			FVector p = voxel_data.voxelIndexToVector(x, y, z);
			p += voxel_data.getOrigin();
			if (p.Z > voxel_data_param.z_cut_level) {
				return 0;
			}
		}

		return voxel_data.getDensity(x, y, z);
	}

	FORCEINLINE int getMaterial(int x, int y, int z) {
		return voxel_data.getMaterial(x, y, z);
	}

	FORCEINLINE FVector vertexInterpolation(FVector p1, FVector p2, float valp1, float valp2) {
		if (std::abs(isolevel - valp1) < 0.00001) {
			return p1;
		}

		if (std::abs(isolevel - valp2) < 0.00001) {
			return p2;
		}

		if (std::abs(valp1 - valp2) < 0.00001) {
			return p1;
		}

		if (valp1 == valp2) {
			return p1;
		}

		float mu = (isolevel - valp1) / (valp2 - valp1);

		FVector tmp;
		tmp.X = p1.X + mu * (p2.X - p1.X);
		tmp.Y = p1.Y + mu * (p2.Y - p1.Y);
		tmp.Z = p1.Z + mu * (p2.Z - p1.Z);

		return tmp;
	}

	FORCEINLINE void materialCalculation(struct TmpPoint& tp, FVector p1, FVector p2, FVector p, int mat1, int mat2) {
		int base_mat = 1;

		if ((base_mat == mat1)&&(base_mat == mat2)) {
			tp.mat_id = base_mat;
			tp.mat_weight = 0;
			return;
		}

		if (mat1 == mat2) {
			tp.mat_id = base_mat;
			tp.mat_weight = 1;
			return;
		}

		FVector tmp(p1);
		tmp -= p2;
		float s = tmp.Size();

		p1 -= p;
		p2 -= p;

		float s1 = p1.Size();
		float s2 = p2.Size();

		if (mat1 != base_mat) {
			tp.mat_weight = s1 / s;
			return;
		}

		if (mat2 != base_mat) {
			tp.mat_weight = s2 / s;
			return;
		}
	}

	FORCEINLINE TmpPoint vertexClc(FVector p1, FVector p2, Point valp1, Point valp2) {
		struct TmpPoint ret;

		ret.v = vertexInterpolation(p1, p2, valp1.density, valp2.density);
		materialCalculation(ret, p1, p2, ret.v, valp1.material_id, valp2.material_id);
		return ret;
	}

    TMap<FVector, int> vertex_map;
    
    FORCEINLINE void addVertex(TmpPoint &point, FVector n, int &index){
		FVector v = point.v;

        if(vertex_map.Contains(v)){
            int vindex = vertex_map[v];
            
            FVector nvert = mesh_data.normals[vindex];
            
            //FVector tmp(0,0,1);
            FVector tmp(nvert);
            tmp += n;
            tmp /= 2;
                    
            //mesh_data.normals.Insert(tmp, vindex);   
            mesh_data.normals[vindex] = tmp;
            mesh_data.tris.Add(vindex);
        } else {
            mesh_data.normals.Emplace(n);
            mesh_data.verts.Add(v);        
            mesh_data.tris.Add(index);

			int t = point.mat_weight * 255;
			mesh_data.colors.Add(FColor(t, 0, 0, 0));
        
            vertex_map.Add(v, index);          
            vertex_index++;  
        }
    }

    FORCEINLINE void handleTriangle(TmpPoint &tmp1, TmpPoint &tmp2, TmpPoint &tmp3) {
        //vp1 = vp1 + voxel_data.getOrigin();
        //vp2 = vp2 + voxel_data.getOrigin();
        //vp3 = vp3 + voxel_data.getOrigin();
        
		FVector n = clcNormal(tmp1.v, tmp2.v, tmp3.v);
		n = -n;
        
        addVertex(tmp1, n, vertex_index);
        addVertex(tmp2, n, vertex_index);
        addVertex(tmp3, n, vertex_index);

        ntriang++;
    }

public:
	FORCEINLINE void generateCell(int x, int y, int z) {
        float isolevel = 0.5f;
		Point d[8];

		int step = voxel_data_param.lod;

        d[0] = getVoxelpoint(x + step, y + step, z);
        d[1] = getVoxelpoint(x + step, y, z);
        d[2] = getVoxelpoint(x, y, z);
        d[3] = getVoxelpoint(x, y + step, z);
        d[4] = getVoxelpoint(x + step, y + step, z + step);
        d[5] = getVoxelpoint(x + step, y, z + step);
        d[6] = getVoxelpoint(x, y, z + step);
        d[7] = getVoxelpoint(x, y + step, z + step);

        int cubeindex = 0;
        if (d[0].density < isolevel)
            cubeindex |= 1;
        if (d[1].density < isolevel)
            cubeindex |= 2;
        if (d[2].density < isolevel)
            cubeindex |= 4;
        if (d[3].density < isolevel)
            cubeindex |= 8;
        if (d[4].density < isolevel)
            cubeindex |= 16;
        if (d[5].density < isolevel)
            cubeindex |= 32;
        if (d[6].density < isolevel)
            cubeindex |= 64;
        if (d[7].density < isolevel)
            cubeindex |= 128;

        int edge = edgeTable[cubeindex];
        if (edge == 0) {
            return;
        }

        FVector p[8];
        p[0] = voxel_data.voxelIndexToVector(x + step, y + step, z);
        p[1] = voxel_data.voxelIndexToVector(x + step, y, z);
        p[2] = voxel_data.voxelIndexToVector(x, y, z);
        p[3] = voxel_data.voxelIndexToVector(x, y + step, z);
        p[4] = voxel_data.voxelIndexToVector(x + step, y + step, z + step);
        p[5] = voxel_data.voxelIndexToVector(x + step, y, z + step);
        p[6] = voxel_data.voxelIndexToVector(x, y, z + step);
        p[7] = voxel_data.voxelIndexToVector(x, y + step, z + step);

		struct TmpPoint vertex_list[12];

        if ((edge & 1) != 0) {
            vertex_list[0] = vertexClc(p[0], p[1], d[0], d[1]);
        }

        if ((edge & 2) != 0) {
            vertex_list[1] = vertexClc(p[1], p[2], d[1], d[2]);
        }

        if ((edge & 4) != 0) {
            vertex_list[2] = vertexClc(p[2], p[3], d[2], d[3]);
        }

        if ((edge & 8) != 0) {
            vertex_list[3] = vertexClc(p[3], p[0], d[3], d[0]);
        }

        if ((edge & 16) != 0) {
            vertex_list[4] = vertexClc(p[4], p[5], d[4], d[5]);
        }

        if ((edge & 32) != 0) {
            vertex_list[5] = vertexClc(p[5], p[6], d[5], d[6]);
        }

        if ((edge & 64) != 0) {
            vertex_list[6] = vertexClc(p[6], p[7], d[6], d[7]);
        }

        if ((edge & 128) != 0) {
            vertex_list[7] = vertexClc(p[7], p[4], d[7], d[4]);
        }

        if ((edge & 256) != 0) {
            vertex_list[8] = vertexClc(p[0], p[4], d[0], d[4]);
        }

        if ((edge & 512) != 0) {
            vertex_list[9] = vertexClc(p[1], p[5], d[1], d[5]);
        }

        if ((edge & 1024) != 0) {
            vertex_list[10] = vertexClc(p[2], p[6], d[2], d[6]);
        }

        if ((edge & 2048) != 0) {
            vertex_list[11] = vertexClc(p[3], p[7], d[3], d[7]);
        }

        for (int i = 0; triTable[cubeindex][i] != -1; i += 3) {
			TmpPoint tmp1 = vertex_list[triTable[cubeindex][i]];
			TmpPoint tmp2 = vertex_list[triTable[cubeindex][i + 1]];
			TmpPoint tmp3 = vertex_list[triTable[cubeindex][i + 2]];

			handleTriangle(tmp1, tmp2, tmp3);
        }
    }
};

void sandboxVoxelGenerateMesh(MeshDataElement &mesh_data, const VoxelData &vd, const VoxelDataParam &vdp) {
    VoxelMeshProcessor vp(mesh_data, vd, vdp);

	int step = vdp.lod;

    for (int x = 0; x < vd.num() - step; x += step) {
        for (int y = 0; y < vd.num() - step; y += step) {
            for (int z = 0; z < vd.num() - step; z += step) {
                vp.generateCell(x, y, z);
            }
        }
    }

    mesh_data.triangle_count = vp.ntriang;
    mesh_data.vertex_count = vp.vertex_index;
}

// =================================================================
// terrain
// =================================================================

#include <mutex>
//#include <map>
static std::mutex terrain_map_mutex;
static TMap<FVector, VoxelData*> terrain_zone_map;


void sandboxRegisterTerrainVoxelData(VoxelData* vd, FVector index) {
	terrain_map_mutex.lock();
	terrain_zone_map.Add(index, vd);
	terrain_map_mutex.unlock();
}

VoxelData* sandboxGetTerrainVoxelDataByPos(FVector point) {
	FVector index = sandboxSnapToGrid(point, 1000) / 1000;

	terrain_map_mutex.lock();
	if (terrain_zone_map.Contains(index)) {
		VoxelData* vd = terrain_zone_map[index];
		terrain_map_mutex.unlock();
		return vd;
	}

	terrain_map_mutex.unlock();
	return NULL;
}

VoxelData* sandboxGetTerrainVoxelDataByIndex(FVector index) {
	terrain_map_mutex.lock();
	if (terrain_zone_map.Contains(index)) {
		VoxelData* vd = terrain_zone_map[index];
		terrain_map_mutex.unlock();
		return vd;
	}

	terrain_map_mutex.unlock();
	return NULL;
}


void sandboxSaveVoxelData(const VoxelData &vd, FString &fullFileName) {
	FBufferArchive binaryData;
	int32 num = vd.num();
	float size = vd.size();
	unsigned char volume_state = 0;


	binaryData << num;
	binaryData << size;

	// save density
	if (vd.getDensityFillState() == VoxelDataFillState::ZERO) {
		volume_state = 0;
		binaryData << volume_state;
	}

	if (vd.getDensityFillState() == VoxelDataFillState::ALL) {
		volume_state = 1;
		binaryData << volume_state;
	}

	if (vd.getDensityFillState() == VoxelDataFillState::MIX) {
		volume_state = 2;
		binaryData << volume_state;
		for (int x = 0; x < num; x++) {
			for (int y = 0; y < num; y++) {
				for (int z = 0; z < num; z++) {
					VoxelPoint vp = vd.getVoxelPoint(x, y, z);
					unsigned char density = vp.density;
					binaryData << density;
				}
			}
		}
	}

	// save material
	if (vd.material_data == NULL) {
		volume_state = 0;
	} else {
		volume_state = 2;
	}

	unsigned char base_mat = vd.base_fill_mat;

	binaryData << volume_state;
	binaryData << base_mat;

	if (volume_state == 2) {
		for (int x = 0; x < num; x++) {
			for (int y = 0; y < num; y++) {
				for (int z = 0; z < num; z++) {
					VoxelPoint vp = vd.getVoxelPoint(x, y, z);
					unsigned char mat_id = vp.material;
					binaryData << mat_id;
				}
			}
		}
	}

	int32 end_marker = 666999;
	binaryData << end_marker;

	if (FFileHelper::SaveArrayToFile(binaryData, *fullFileName)) {
		binaryData.FlushCache();
		binaryData.Empty();
	}
}

bool sandboxLoadVoxelData(VoxelData &vd, FString &fullFileName) {
	double start = FPlatformTime::Seconds();

	TArray<uint8> TheBinaryArray;
	if (!FFileHelper::LoadFileToArray(TheBinaryArray, *fullFileName)) {
		UE_LOG(LogTemp, Warning, TEXT("Zone file not found -> %s"), *fullFileName);
		return false;
	}
	
	if (TheBinaryArray.Num() <= 0) return false;

	FMemoryReader binaryData = FMemoryReader(TheBinaryArray, true); //true, free data after done
	binaryData.Seek(0);
	
	int32 num;
	float size;
	unsigned char volume_state;
	unsigned char base_mat;

	binaryData << num;
	binaryData << size;

	// load density
	binaryData << volume_state;

	if (volume_state == 0) {
		vd.deinitializeDensity(VoxelDataFillState::ZERO);
	}

	if (volume_state == 1) {
		vd.deinitializeDensity(VoxelDataFillState::ALL);
	}

	if (volume_state == 2) {
		for (int x = 0; x < num; x++) {
			for (int y = 0; y < num; y++) {
				for (int z = 0; z < num; z++) {
					unsigned char density;
					binaryData << density;
					vd.setVoxelPointDensity(x, y, z, density);
				}
			}
		}
	}
	
	// load material
	binaryData << volume_state;
	binaryData << base_mat;
	if (volume_state == 2) {
		for (int x = 0; x < num; x++) {
			for (int y = 0; y < num; y++) {
				for (int z = 0; z < num; z++) {
					unsigned char mat_id;
					binaryData << mat_id;
					vd.setVoxelPointMaterial(x, y, z, mat_id);
				}
			}
		}
	} else {
		vd.deinitializeMaterial(base_mat);
	}

	int32 end_marker;
	binaryData << end_marker;
	
	binaryData.FlushCache();
	TheBinaryArray.Empty();
	binaryData.Close();
	
	double end = FPlatformTime::Seconds();
	double time = (end - start) * 1000;
	//UE_LOG(LogTemp, Warning, TEXT("Loading terrain zone: %s -> %f ms"), *fileFullPath, time);
	
	return true;
}


extern FVector sandboxConvertVectorToCubeIndex(FVector vec) {
	return sandboxSnapToGrid(vec, 200);
}

extern FVector sandboxSnapToGrid(FVector vec, float grid_range) {
	FVector tmp(vec);
	tmp /= grid_range;
	//FVector tmp2(std::round(tmp.X), std::round(tmp.Y), std::round(tmp.Z));
	FVector tmp2((int)tmp.X, (int)tmp.Y, (int)tmp.Z);
	tmp2 *= grid_range;
	return FVector((int)tmp2.X, (int)tmp2.Y, (int)tmp2.Z);
}

FVector sandboxGridIndex(FVector v, int range) {
	FVector tmp = sandboxSnapToGrid(v, range) / range;
	return FVector((int)tmp.X, (int)tmp.Y, (int)tmp.Z);
}
