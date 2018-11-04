#ifndef __SANDBOXMOBILE_VOXELDATA_H__
#define __SANDBOXMOBILE_VOXELDATA_H__

#include "EngineMinimal.h"
#include "VoxelData.h"
#include "ProcMeshData.h"
#include "VoxelMeshData.h"


std::shared_ptr<TMeshData> sandboxVoxelGenerateMesh(const TVoxelData &vd, const TVoxelDataParam &vdp);

extern FVector sandboxSnapToGrid(FVector vec, float grid_range);
extern FVector sandboxConvertVectorToCubeIndex(FVector vec);

extern FVector sandboxGridIndex(const FVector& v, int range);

#endif