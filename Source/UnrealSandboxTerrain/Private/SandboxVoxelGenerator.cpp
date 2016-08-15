
#include "UnrealSandboxTerrainPrivatePCH.h"
#include "SandboxVoxelGenerator.h"
#include "SandboxVoxeldata.h"
#include "SandboxPerlinNoise.h"


usand::PerlinNoise pn;

float groundLevel(FVector v) {
	//float scale1 = 0.0035f; // small
	float scale1 = 0.0015f; // small
	float scale2 = 0.0004f; // medium
	float scale3 = 0.00009f; // big

	float noise_small = pn.noise(v.X * scale1, v.Y * scale1, 0);
	float noise_medium = pn.noise(v.X * scale2, v.Y * scale2, 0) * 5;
	float noise_big = pn.noise(v.X * scale3, v.Y * scale3, 0) * 15;
	float gl = noise_medium + noise_small + noise_big;

	gl = gl * 100;

	return gl;
}

float densityByGroundLevel(FVector v) {
	float gl = groundLevel(v);
	float val = 1;

	if (v.Z > gl + 400) {
		val = 0;
	}
	else if (v.Z > gl) {
		float d = (1 / (v.Z - gl)) * 100;
		val = d;
	}

	if (val > 1) {
		val = 1;
	}

	if (val < 0.003) { // minimal density = 1f/255
		val = 0;
	}

	return val;
}


float funcGenerateCavern(float density, FVector v) {
	float den = density;
	float r = FMath::Sqrt(v.X * v.X + v.Y * v.Y + v.Z * v.Z);
	if (r < 300) {
		float scale1 = 0.01f; // small
		float scale2 = 0.005f; // medium
		float noise_medium = FMath::Abs(pn.noise(v.X * scale2, v.Y * scale2, v.Z * scale2));
		float noise_small = FMath::Abs(pn.noise(v.X * scale1, v.Y * scale1, v.Z * scale1));

		//den -= noise_medium;
		den -= noise_small * 0.15 + noise_medium * 0.3 + (1 / r) * 100;
		if (den < 0) {
			den = 0;
		}
	}

	return den;
}

FORCEINLINE unsigned long vectorHash(FVector v) {
	return ((int)v.X * 73856093) ^ ((int)v.Y * 19349663) ^ ((int)v.Z * 83492791);
}

float clcGroundLevelDelta(FVector v) {
	return groundLevel(v) - v.Z;
}


SandboxVoxelGenerator::SandboxVoxelGenerator(VoxelData& vd) {
	int32 zone_seed = vectorHash(vd.getOrigin());

	FRandomStream rnd = FRandomStream();
	rnd.Initialize(zone_seed);
	rnd.Reset();

	this->cavern = false;
	float gl_delta = clcGroundLevelDelta(vd.getOrigin());
	if (rnd.FRandRange(0.f, 1.f) > 0.95 || (vd.getOrigin().X == 0 && vd.getOrigin().Y == 0)) {
		if (gl_delta > 500 && gl_delta < 2000) {
			this->cavern = true;
		}
	}
}

SandboxVoxelGenerator::~SandboxVoxelGenerator() {

}

float SandboxVoxelGenerator::density(FVector& local, FVector& world) {
	float den = densityByGroundLevel(world);

	// ==============================================================
	// cavern
	// ==============================================================
	if (this->cavern) {
		den = funcGenerateCavern(den, local);
	}
	// ==============================================================

	return den;
}

unsigned char SandboxVoxelGenerator::material(FVector& local, FVector& world) {
	FVector test2 = FVector(world);
	test2.Z += 30;

	float den2 = densityByGroundLevel(test2);

	unsigned char mat = 0;
	if (den2 < 0.5) {
		mat = 2;
	}
	else {
		mat = 1;
	}

	return mat;
}