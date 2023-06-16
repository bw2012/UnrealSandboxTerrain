#pragma once

#include "EngineMinimal.h"
#include "VoxelIndex.h"

//======================================================================================================================================================================
//
//======================================================================================================================================================================

struct TTerrainAreaLoadParams {
	TTerrainAreaLoadParams() {};

	TTerrainAreaLoadParams(uint32 AreaSize, uint32 AreaDepth) {
		Radius = (float)(AreaSize * USBT_ZONE_SIZE);
		TerrainSizeMinZ = -(float)(AreaDepth);
		TerrainSizeMaxZ = (float)(AreaDepth);
	};

	float Radius = 3000;
	int32 TerrainSizeMinZ = -5;
	int32 TerrainSizeMaxZ = 5;

	std::function<void(uint32, uint32)> OnProgress = nullptr;
};


class TTerrainAreaHelper {

public:

	TTerrainAreaHelper() {}

	virtual ~TTerrainAreaHelper() { 

	}

	TTerrainAreaHelper(FString Name_, ASandboxTerrainController* Controller_) :
		Name(Name_), Controller(Controller_) {}

	TTerrainAreaHelper(FString Name_, ASandboxTerrainController* Controller_, TTerrainAreaLoadParams Params_) :
		Name(Name_), Controller(Controller_), Params(Params_) {
	}

protected:
	FString Name;
	ASandboxTerrainController* Controller;
	TTerrainAreaLoadParams Params;
	FVector AreaOrigin;
	TVoxelIndex OriginIndex;
	uint32 Total = 0;
	uint32 Progress = 0;
	bool bIsStopped = false;

protected:

	virtual void PerformZone(const TVoxelIndex& Index) {

	}

	virtual void EndChunk(int X, int Y) {
		Controller->GetTerrainGenerator()->Clean(TVoxelIndex(X, Y, 0));
	}

	virtual void BeginChunk(int X, int Y) {

	}

private:

	void PerformChunk(int X, int Y) {
		for (int Z = Params.TerrainSizeMinZ; Z <= Params.TerrainSizeMaxZ; Z++) {
			TVoxelIndex Index(X + OriginIndex.X, Y + OriginIndex.Y, Z + OriginIndex.Z);

			PerformZone(Index);
			Progress++;

			if (Params.OnProgress) {
				Params.OnProgress(Progress, Total);
			}

			if (Controller->IsWorkFinished() || bIsStopped) {
				return;
			}
		}
	}

	void AreaWalkthrough() {
		const unsigned int AreaRadius = Params.Radius / 1000;
		Total = (AreaRadius * 2 + 1) * (AreaRadius * 2 + 1) * (Params.TerrainSizeMinZ + Params.TerrainSizeMaxZ + 1);
		auto List = ReverseSpiralWalkthrough(AreaRadius);
		int Idx = 1;
		for (auto& Itm : List) {
			int RelX = Itm.X;
			int RelY = Itm.Y;

			if (Idx % 100 == 0 || Idx == List.size()) {
				const float P = ((float)Idx / (float)Total) * 100;
				UE_LOG(LogVt, Log, TEXT("Chunk loader '%s': process chunk %d / %d - %.1f%%"), *Name, Idx, List.size(), P);
			}

			BeginChunk(RelX + OriginIndex.X, RelY + OriginIndex.Y);
			PerformChunk(RelX, RelY);
			EndChunk(RelX + OriginIndex.X, RelY + OriginIndex.Y);

			if (Controller->IsWorkFinished() || bIsStopped) {
				return;
			}

			Idx++;
		}
	}

public:

	void Cancel() {
		this->bIsStopped = true;
	}

	void SetParams(FString NewName, ASandboxTerrainController* NewController, TTerrainAreaLoadParams NewParams) {
		this->Name = NewName;
		this->Controller = NewController;
		this->Params = NewParams;
	}

	void LoadArea(const FVector& Origin) {
		if (this->Controller) {
			this->AreaOrigin = Origin;
			this->OriginIndex = Controller->GetZoneIndex(Origin);
			AreaWalkthrough();
		}
	}

	void LoadArea(const TVoxelIndex& ZoneIndex) {
		if (this->Controller) {
			this->AreaOrigin = Controller->GetZonePos(ZoneIndex);
			this->OriginIndex = ZoneIndex;
			AreaWalkthrough();
		}
	}
};


class TTerrainLoadHelper : public TTerrainAreaHelper  {

public:

	using TTerrainAreaHelper::TTerrainAreaHelper;

protected :

	virtual void PerformZone(const TVoxelIndex& Index) override {
		if (Controller->TerrainData->IsOutOfSync(Index)) {
			return;
		}

		TArray<TSpawnZoneParam> SpawnList;
		TSpawnZoneParam SpawnZoneParam;
		SpawnZoneParam.Index = Index;
		SpawnList.Add(SpawnZoneParam);

		// batch with one zone. CPU only
		Controller->BatchSpawnZone(SpawnList);
	}
};

class TCheckAreaMap {
public:
	TMap<uint32, std::shared_ptr<TTerrainLoadHelper>> PlayerStreamingHandler;
	TMap<uint32, FVector> PlayerStreamingPosition;
};
