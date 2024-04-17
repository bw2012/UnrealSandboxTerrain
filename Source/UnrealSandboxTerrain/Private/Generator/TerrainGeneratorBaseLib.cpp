// dd2012 10.04.2024


#include "TerrainGeneratorComponent.h"
#include "SandboxTerrainController.h"
#include "Core/perlin.hpp"
#include <algorithm>
#include <thread>
#include <atomic>
#include "Math/UnrealMathUtility.h"

#include "TerrainZoneComponent.h"


// TODO noise factor
float UTerrainGeneratorComponent::FunctionMakeSphere(const float InDensity, const FVector& V, const FVector& Origin, const float Radius, const float NoiseFactor) const {
	static const float E = 50;
	static const float NoisePosScale = 0.007f / 2;
	static const float NoiseScale = 0.18;

	if (InDensity > 0.5f) {
		const FVector P = V - Origin;
		const float R = std::sqrt(P.X * P.X + P.Y * P.Y + P.Z * P.Z);
		if (R < Radius + E) {
			if (R < Radius - E) {
				return 0.f;
			}
			else {
				const float N = PerlinNoise(P, NoisePosScale, NoiseScale);
				float Density = 1 / (1 + exp((Radius - R) / 100)) + N;
				if (Density < InDensity) {
					return Density;
				}
			}
		}
	}

	return InDensity;
};

TGenerationResult UTerrainGeneratorComponent::FunctionMakeSolidSphere(const float InDensity, const TMaterialId InMaterialId, const FVector& V, const FVector& Origin, const float Radius, const TMaterialId ShellMaterialId) const {
	static const float E = 50;
	static const float Thickness = 100;

	const FVector P = V - Origin;
	const float R = std::sqrt(P.X * P.X + P.Y * P.Y + P.Z * P.Z);

	float Density = InDensity;
	TMaterialId MaterialId = InMaterialId;

	if (R < Radius + Thickness && R > Radius - Thickness) {
		MaterialId = ShellMaterialId;
		Density = 1;
	}

	if (R < Radius + E - Thickness) {
		Density = 1 / (1 + exp((Radius - R - Thickness) / 100));
		return std::make_tuple(Density, MaterialId);
	}

	if (R < Radius - E - Thickness) {
		Density = 0;
		return std::make_tuple(Density, MaterialId);
	}

	TVoxelIndex Index = GetController()->GetZoneIndex(V);
	if (R > Radius) {
		if (R > Radius + Thickness + E * 2) {
			Density = InDensity;
		}
		else {
			const float D = exp((Radius - R) / 100);
			Density = D;
			if (R > Radius + E) {
				if (InDensity > D) {
					Density = InDensity;
				}
			}
		}
	}

	return std::make_tuple(Density, MaterialId);
};

float UTerrainGeneratorComponent::FunctionMakeVerticalCylinder(const float InDensity, const FVector& V, const FVector& Origin, const float Radius, const float Top, const float Bottom, const float NoiseFactor) const {
	static const float E = 50;
	static const float NoisePositionScale = 0.007f;
	const float NoiseValueScale = 0.1 * NoiseFactor;

	if (InDensity > 0.5f) {
		if (V.Z < (Origin.Z + Top + E) && V.Z >(Origin.Z + Bottom - E)) {
			const FVector P = V - Origin;
			const float R = std::sqrt(P.X * P.X + P.Y * P.Y);
			if (R < Radius + E) {
				if (R < Radius - E) {
					return 0.f;
				} else {
					const float N = PerlinNoise(P, NoisePositionScale, NoiseValueScale);
					float Density = 1 / (1 + exp((Radius - R) / 100)) + N;
					if (Density < InDensity) {
						return Density;
					}
				}
			}
		}
	}

	return InDensity;
};

float UTerrainGeneratorComponent::FunctionMakeBox(const float InDensity, const FVector& P, const FBox& InBox) const {
	static const float E = 50;

	const float ExtendXP = InBox.Max.X;
	const float ExtendYP = InBox.Max.Y;
	const float ExtendZP = InBox.Max.Z;
	const float ExtendXN = InBox.Min.X;
	const float ExtendYN = InBox.Min.Y;
	const float ExtendZN = InBox.Min.Z;

	const FBox Box = InBox.ExpandBy(E);

	static float D = 100;
	static const float NoisePositionScale = 0.005f;
	static const float NoiseValueScale1 = 0.145;
	static const float NoiseValueScale2 = 0.08;
	float R = InDensity;

	if (InDensity < 0.5f) {
		return InDensity;
	}

	if (FMath::PointBoxIntersection(P, Box)) {
		R = 0;

		if (FMath::Abs(P.X - ExtendXP) < 50 || FMath::Abs(-P.X + ExtendXN) < 50) {
			const float DensityXP = 1 / (1 + exp((ExtendXP - P.X) / D));
			const float DensityXN = 1 / (1 + exp((-ExtendXN + P.X) / D));
			const float DensityX = (DensityXP + DensityXN);
			const float N = PerlinNoise(P, NoisePositionScale, NoiseValueScale1);
			R = DensityX + N;
		}

		if (FMath::Abs(P.Y - ExtendYP) < 50 || FMath::Abs(-P.Y + ExtendYN) < 50) {
			if (R < 0.5f) {
				const float DensityYP = 1 / (1 + exp((ExtendYP - P.Y) / D));
				const float DensityYN = 1 / (1 + exp((-ExtendYN + P.Y) / D));
				const float DensityY = (DensityYP + DensityYN);
				const float N = PerlinNoise(P, NoisePositionScale, NoiseValueScale1);
				R = DensityY + N;
			}
		}

		const float NPZ = NormalizedPerlinNoise(FVector(P.X, P.Y, 0), 0.0005f, 400) + NormalizedPerlinNoise(FVector(P.X, P.Y, 0), 0.0005f * 4, 100);

		if (FMath::Abs(P.Z - ExtendZP) < 300 || FMath::Abs(-P.Z + ExtendZN) < 50) {
			if (R < 0.5f) {
				const float DensityZP = 1 / (1 + exp((ExtendZP - NPZ - P.Z) / D));
				const float DensityZN = 1 / (1 + exp((-ExtendZN + P.Z) / D));
				const float DensityZ = (DensityZP + DensityZN);
				const float N = PerlinNoise(P, NoisePositionScale, NoiseValueScale2);
				R = DensityZ + N;
			}
		}
	}

	if (R > 1) {
		R = 1;
	}

	if (R < 0) {
		R = 0;
	}

	return R;
};