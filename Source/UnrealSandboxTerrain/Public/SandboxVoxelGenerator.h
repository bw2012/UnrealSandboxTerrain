#pragma once

#include "EngineMinimal.h"

class VoxelData;

class SandboxVoxelGenerator {

public:
	SandboxVoxelGenerator(VoxelData& vd);

	~SandboxVoxelGenerator();

	virtual float density(FVector& local, FVector& world);

	virtual unsigned char material(FVector& local, FVector& world);

private:

	bool cavern;
};