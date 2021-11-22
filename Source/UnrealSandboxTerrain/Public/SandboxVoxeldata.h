#ifndef __SANDBOXMOBILE_VOXELDATA_H__
#define __SANDBOXMOBILE_VOXELDATA_H__

#include "EngineMinimal.h"
#include "VoxelData.h"
#include "ProcMeshData.h"
#include "VoxelMeshData.h"


std::shared_ptr<TMeshData> sandboxVoxelGenerateMesh(const TVoxelData &vd, const TVoxelDataParam &vdp);

TMeshDataPtr polygonizeSingleCell(const TVoxelData& vd, const TVoxelDataParam& vdp, int x, int y, int z);

#endif
