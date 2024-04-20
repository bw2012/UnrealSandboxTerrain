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

void StructureHotizontalBoxTunnel(TStructuresGenerator* Generator, const FBox TunnelBox, TSet<TVoxelIndex>& Res) {

	const UTerrainGeneratorComponent* Generator2 = (UTerrainGeneratorComponent*)Generator->GetGeneratorComponent();

	const auto Function = [=](const float InDensity, const TMaterialId InMaterialId, const TVoxelIndex& VoxelIndex, const FVector& LocalPos, const FVector& WorldPos) {
		const float Density = Generator2->FunctionMakeBox(InDensity, WorldPos, TunnelBox);
		const TMaterialId MaterialId = InMaterialId;
		return std::make_tuple(Density, MaterialId);
	};

	TZoneStructureHandler Str;
	Str.Function = Function;
	//Str.Box = TunnelBox;

	const TVoxelIndex MinIndex = Generator->GetController()->GetZoneIndex(TunnelBox.Min);
	const TVoxelIndex MaxIndex = Generator->GetController()->GetZoneIndex(TunnelBox.Max);

	for (auto X = MinIndex.X; X <= MaxIndex.X; X++) {
		for (auto Y = MinIndex.Y; Y <= MaxIndex.Y; Y++) {
			const TVoxelIndex Index(X, Y, MinIndex.Z);
			Generator->AddZoneStructure(Index, Str);
			Res.Add(Index);
		}
	}

}

void StructureVerticalCylinderTunnel(TStructuresGenerator* Generator, const FVector& Origin, const float Radius, const float Top, const float Bottom) {

	const UTerrainGeneratorComponent* Generator2 = (UTerrainGeneratorComponent*)Generator->GetGeneratorComponent();

	const auto Function = [=](const float InDensity, const TMaterialId InMaterialId, const TVoxelIndex& VoxelIndex, const FVector& LocalPos, const FVector& WorldPos) {
		const float Density = Generator2->FunctionMakeVerticalCylinder(InDensity, WorldPos, Origin, Radius, Top, Bottom);
		return std::make_tuple(Density, InMaterialId);
	};

	const auto Function2 = [=](const TVoxelIndex& ZoneIndex, const FVector& WorldPos, const FVector& LocalPos) {
		const FVector P = WorldPos - Origin;
		const float R = std::sqrt(P.X * P.X + P.Y * P.Y);

		if (R < Radius) {
			return false;
		}
		return true;
	};

	TZoneStructureHandler Str;
	Str.Function = Function;
	Str.LandscapeFoliageFilter = Function2;
	Str.Pos = Origin;

	FVector Min(Origin);
	Min.Z += Bottom;

	FVector Max(Origin);
	Max.Z += Top;

	const TVoxelIndex MinIndex = Generator->GetController()->GetZoneIndex(Min);
	const TVoxelIndex MaxIndex = Generator->GetController()->GetZoneIndex(Max);

	for (auto Z = MinIndex.Z; Z <= MaxIndex.Z; Z++) {
		Generator->AddZoneStructure(TVoxelIndex(MinIndex.X, MinIndex.Y, Z), Str);
		//AsyncTask(ENamedThreads::GameThread, [=]() { DrawDebugBox(Generator->GetController()->GetWorld(), Generator->GetController()->GetZonePos(TVoxelIndex(MinIndex.X, MinIndex.Y, Z)), FVector(USBT_ZONE_SIZE / 2), FColor(255, 255, 255, 0), true); });
	}
}

void StructureDiagonalCylinderTunnel(TStructuresGenerator* Generator, const FVector& Origin, const float Radius, const float Top, const float Bottom, const int Dir) {
	static const FRotator DirRotation[2] = { FRotator(-45, 0, 0), FRotator(0, 0, -45) };
	static const TVoxelIndex DirIndex[2] = { TVoxelIndex(1, 0, 0), TVoxelIndex(0, -1, 0) };

	const UTerrainGeneratorComponent* Generator2 = (UTerrainGeneratorComponent*)Generator->GetGeneratorComponent();

	const auto Function = [=](const float InDensity, const TMaterialId InMaterialId, const TVoxelIndex& VoxelIndex, const FVector& LocalPos, const FVector& WorldPos) {
		const FRotator& Rotator = DirRotation[Dir];
		FVector Tmp = Rotator.RotateVector(WorldPos - Origin);
		Tmp += Origin;

		const static float Sqrt2 = 1.414213;

		const float Density = Generator2->FunctionMakeVerticalCylinder(InDensity, Tmp, Origin, Radius, Top, Bottom * Sqrt2 - 350, 0.66);

		return std::make_tuple(Density, InMaterialId);
	};

	const auto Function2 = [=](const TVoxelIndex& ZoneIndex, const FVector& WorldPos, const FVector& LocalPos) {
		const FRotator& Rotator = DirRotation[Dir];
		FVector Tmp = Rotator.RotateVector(WorldPos - Origin);
		Tmp += Origin;

		static const float E = 25;
		if (Tmp.Z < Top + E && Tmp.Z > Bottom - E) {
			const FVector P = Tmp - Origin;
			const float R = std::sqrt(P.X * P.X + P.Y * P.Y);
			if (R < Radius + E) {
				return false;
			}
		}

		return true;
	};

	TZoneStructureHandler Str;
	Str.Function = Function;
	Str.LandscapeFoliageFilter = Function2;
	Str.Pos = Origin;

	FVector Min(Origin);
	Min.Z += Bottom;

	FVector Max(Origin);
	//Max.Z += Top;

	const TVoxelIndex MinIndex = Generator->GetController()->GetZoneIndex(Min);
	const TVoxelIndex MaxIndex = Generator->GetController()->GetZoneIndex(Max);

	int T = -1;
	for (auto Z = MaxIndex.Z + 1; Z >= MinIndex.Z; Z--) {
		const TVoxelIndex TV = DirIndex[Dir] * T;
		const TVoxelIndex TV1 = (DirIndex[Dir] * T) + DirIndex[Dir];
		const TVoxelIndex TV2 = (DirIndex[Dir] * T) - DirIndex[Dir];

		const TVoxelIndex Index0(MinIndex.X + TV.X, MinIndex.Y + TV.Y, Z + TV.Z);
		const TVoxelIndex Index1(MinIndex.X + TV1.X, MinIndex.Y + TV1.Y, Z + TV1.Z);
		const TVoxelIndex Index2(MinIndex.X + TV2.X, MinIndex.Y + TV2.Y, Z + TV2.Z);

		Generator->AddZoneStructure(Index0, Str);
		Generator->AddZoneStructure(Index1, Str);
		Generator->AddZoneStructure(Index2, Str);

		AsyncTask(ENamedThreads::GameThread, [=]() {
			const FVector Pos0 = Generator->GetController()->GetZonePos(Index0);
			const FVector Pos1 = Generator->GetController()->GetZonePos(Index1);
			const FVector Pos2 = Generator->GetController()->GetZonePos(Index2);

			//DrawDebugBox(Generator->GetController()->GetWorld(), Pos0, FVector(USBT_ZONE_SIZE / 2), FColor(255, 255, 255, 0), true);
			//DrawDebugBox(Generator->GetController()->GetWorld(), Pos1, FVector(USBT_ZONE_SIZE / 2), FColor(255, 255, 255, 0), true);
			//DrawDebugBox(Generator->GetController()->GetWorld(), Pos2, FVector(USBT_ZONE_SIZE / 2), FColor(255, 255, 255, 0), true);
			});

		T++;
	}
}
