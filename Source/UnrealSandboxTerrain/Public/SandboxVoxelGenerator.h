#pragma once

#include "EngineMinimal.h"

class TVoxelData;

class SandboxVoxelGenerator {

public:
	SandboxVoxelGenerator(TVoxelData& vd, int32 Seed);

	~SandboxVoxelGenerator();

	virtual float density(FVector& local, FVector& world);

	virtual unsigned char material(FVector& local, FVector& world);

private:

	bool cavern;
};