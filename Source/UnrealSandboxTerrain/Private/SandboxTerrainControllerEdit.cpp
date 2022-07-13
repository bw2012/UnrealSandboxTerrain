
#include "SandboxTerrainController.h"


struct TZoneEditHandler {
	bool changed = false;
	bool bNoise = false;
	float Strength;

	FVector Origin;
	FRotator Rotator;

	//TTerrainGenerator* Generator;

	static FVector GetVoxelRelativePos(const TVoxelData* Vd, const FVector& Origin, const int X, const int Y, const int Z) {
		FVector Pos = Vd->voxelIndexToVector(X, Y, Z);
		Pos += Vd->getOrigin();
		Pos -= Origin;
		return Pos;
	}

	float Noise(const FVector& Pos) {
		static const float NoisePositionScale = 0.01f;
		static const float NoiseValueScale = 0.5f;
		//const float Noise = Generator->PerlinNoise(Pos, NoisePositionScale, NoiseValueScale);
		const float Noise = 0;
		return Noise;
	}

	float Extend;
};


void ASandboxTerrainController::DigCylinder(const FVector& Origin, const float Radius, const float Length, const FRotator& Rotator, const float Strength, const bool bNoise) {
	struct ZoneHandler : TZoneEditHandler {
		TMap<uint16, FSandboxTerrainMaterial>* MaterialMapPtr;
		float Length;
		float Radius;
		FRotator Rotator;
		bool bNoise;

		bool operator()(TVoxelData* Vd) {
			changed = false;
			bool bIsRotator = !Rotator.IsZero();
			Rotator = Rotator.GetInverse();

			Vd->forEachWithCache([&](int X, int Y, int Z) {
				FVector V = TZoneEditHandler::GetVoxelRelativePos(Vd, Origin, X, Y, Z);
				if (bIsRotator) {
					V = Rotator.RotateVector(V);
				}

				const float R = std::sqrt(V.X * V.X + V.Y * V.Y);

				if (R < Extend + 20 && V.Z < Length && V.Z > -Length) {
					float OldDensity = Vd->getDensity(X, Y, Z);
					float Density = 1 / (1 + exp((Radius - R) / 10));
					//float Density = 1 - exp(-pow(R, 2) / (Radius * 100));

					if (bNoise) {
						Density += Noise(V);
					}

					if (OldDensity > Density) {
						Vd->setDensity(X, Y, Z, Density);
					}

					changed = true;
				}
			}, USBT_ENABLE_LOD);

			return changed;
		}
	} Zh;

	Zh.MaterialMapPtr = &MaterialMap;
	Zh.Strength = Strength;
	Zh.Origin = Origin;
	Zh.Extend = Radius;
	Zh.Radius = Radius;
	Zh.Rotator = Rotator;
	Zh.Length = Length;
	//Zh.Generator = this->Generator;
	Zh.bNoise = bNoise;
	ASandboxTerrainController::PerformTerrainChange(Zh);
}

void ASandboxTerrainController::DigTerrainRoundHole(const FVector& Origin, float Radius, float Strength) {
	if (GetWorld()->IsServer()) {
		//UVdServerComponent* VdServerComponent = Cast<UVdServerComponent>(GetComponentByClass(UVdServerComponent::StaticClass()));
		//VdServerComponent->SendToAllClients(USBT_NET_OPCODE_DIG_ROUND, Origin.X, Origin.Y, Origin.Z, Radius, Strength);
	}
	else {
		//UVdClientComponent* VdClientComponent = Cast<UVdClientComponent>(GetComponentByClass(UVdClientComponent::StaticClass()));
		//VdClientComponent->SendToServer(USBT_NET_OPCODE_DIG_ROUND, Origin.X, Origin.Y, Origin.Z, Radius, Strength);
		return;
	}

	//DigTerrainRoundHole_Internal(Origin, Radius, Strength);

	struct ZoneHandler : TZoneEditHandler {
		TMap<uint16, FSandboxTerrainMaterial>* MaterialMapPtr;

		bool operator()(TVoxelData* vd) {
			changed = false;

			vd->forEachWithCache([&](int x, int y, int z) {
				float density = vd->getDensity(x, y, z);
				FVector o = vd->voxelIndexToVector(x, y, z);
				o += vd->getOrigin();
				o -= Origin;

				float rl = std::sqrt(o.X * o.X + o.Y * o.Y + o.Z * o.Z);
				if (rl < Extend) {
					unsigned short  MatId = vd->getMaterial(x, y, z);
					FSandboxTerrainMaterial& Mat = MaterialMapPtr->FindOrAdd(MatId);

					if (Mat.RockHardness < USBT_MAX_MATERIAL_HARDNESS) {
						float ClcStrength = (Mat.RockHardness == 0) ? Strength : (Strength / Mat.RockHardness);
						if (ClcStrength > 0.1) {
							float d = density - 1 / rl * (ClcStrength);
							vd->setDensity(x, y, z, d);
						}
					}

					changed = true;
				}
			}, USBT_ENABLE_LOD);

			return changed;
		}
	} Zh;

	Zh.MaterialMapPtr = &MaterialMap;
	Zh.Strength = Strength;
	Zh.Origin = Origin;
	Zh.Extend = Radius;
	ASandboxTerrainController::PerformTerrainChange(Zh);

}

void ASandboxTerrainController::DigTerrainCubeHole(const FVector& Origin, const FBox& Box, float Extend, const FRotator& Rotator) {
	if (!GetWorld()->IsServer()) return;

	struct ZoneHandler : TZoneEditHandler {
		TMap<uint16, FSandboxTerrainMaterial>* MaterialMapPtr;
		FRotator Rotator;
		FBox Box;

		bool operator()(TVoxelData* vd) {
			changed = false;
			bool bIsRotator = !Rotator.IsZero();

			vd->forEachWithCache([&](int x, int y, int z) {
				FVector o = vd->voxelIndexToVector(x, y, z);
				o += vd->getOrigin();
				o -= Origin;

				if (bIsRotator) {
					o = Rotator.RotateVector(o);
				}

				bool bIsIntersect = FMath::PointBoxIntersection(o, Box);
				if (bIsIntersect) {
					unsigned short  MatId = vd->getMaterial(x, y, z);
					FSandboxTerrainMaterial& Mat = MaterialMapPtr->FindOrAdd(MatId);
					if (Mat.RockHardness < USBT_MAX_MATERIAL_HARDNESS) {
						//float Density1 = 1 / (1 + exp((Extend - o.X -50) / 10));
						//vd->setDensity(x, y, z, Density1);
						vd->setDensity(x, y, z, 0);
						changed = true;
					}
				}
			}, USBT_ENABLE_LOD);

			return changed;
		}
	} Zh;

	Zh.MaterialMapPtr = &MaterialMap;
	Zh.Origin = Origin;
	Zh.Extend = Extend;
	Zh.Rotator = Rotator;
	Zh.Box = Box;

	ASandboxTerrainController::PerformTerrainChange(Zh);
}


void ASandboxTerrainController::DigTerrainCubeHole(const FVector& Origin, float Extend, const FRotator& Rotator) {
	if (!GetWorld()->IsServer()) return;

	struct ZoneHandler : TZoneEditHandler {
		TMap<uint16, FSandboxTerrainMaterial>* MaterialMapPtr;
		FRotator Rotator;

		bool operator()(TVoxelData* vd) {
			changed = false;

			bool bIsRotator = !Rotator.IsZero();
			FBox Box(FVector(-(Extend + 20)), FVector(Extend + 20));
			vd->forEachWithCache([&](int x, int y, int z) {
				FVector o = vd->voxelIndexToVector(x, y, z);
				o += vd->getOrigin();
				o -= Origin;

				if (bIsRotator) {
					o = Rotator.RotateVector(o);
				}

				bool bIsIntersect = FMath::PointBoxIntersection(o, Box);
				if (bIsIntersect) {
					unsigned short  MatId = vd->getMaterial(x, y, z);
					FSandboxTerrainMaterial& Mat = MaterialMapPtr->FindOrAdd(MatId);
					if (Mat.RockHardness < USBT_MAX_MATERIAL_HARDNESS) {
						const float DensityXP = 1 / (1 + exp((Extend - o.X) / 10));
						const float DensityXN = 1 / (1 + exp((-Extend - o.X) / 10));
						const float DensityYP = 1 / (1 + exp((Extend - o.Y) / 10));
						const float DensityYN = 1 / (1 + exp((-Extend - o.Y) / 10));
						const float DensityZP = 1 / (1 + exp((Extend - o.Z) / 10));
						const float DensityZN = 1 / (1 + exp((-Extend - o.Z) / 10));
						const float Density = DensityXP * DensityXN * DensityYP * DensityYN * DensityZP * DensityZN;
						//float OldDensity = vd->getDensity(x, x, x);

						//if (OldDensity > Density) {
							vd->setDensity(x, y, z, Density);
						//}

						changed = true;
					}
				}
			}, USBT_ENABLE_LOD);

			return changed;
		}
	} Zh;

	Zh.MaterialMapPtr = &MaterialMap;
	Zh.Origin = Origin;
	Zh.Extend = Extend;
	Zh.Rotator = Rotator;
	ASandboxTerrainController::PerformTerrainChange(Zh);
}

void ASandboxTerrainController::FillTerrainCube(const FVector& Origin, float Extend, int MatId) {
	if (!GetWorld()->IsServer()) return;

	struct ZoneHandler : TZoneEditHandler {
		int newMaterialId;
		bool operator()(TVoxelData* vd) {
			changed = false;

			vd->forEachWithCache([&](int x, int y, int z) {
				FVector o = vd->voxelIndexToVector(x, y, z);
				o += vd->getOrigin();
				o -= Origin;
				if (o.X < Extend && o.X > -Extend && o.Y < Extend && o.Y > -Extend && o.Z < Extend && o.Z > -Extend) {
					vd->setDensity(x, y, z, 1);
					changed = true;
				}

				float radiusMargin = Extend + 20;
				if (o.X < radiusMargin && o.X > -radiusMargin && o.Y < radiusMargin && o.Y > -radiusMargin && o.Z < radiusMargin && o.Z > -radiusMargin) {
					vd->setMaterial(x, y, z, newMaterialId);
				}
			}, USBT_ENABLE_LOD);

			return changed;
		}
	} Zh;

	Zh.newMaterialId = MatId;
	Zh.Origin = Origin;
	Zh.Extend = Extend;
	ASandboxTerrainController::PerformTerrainChange(Zh);
}

void ASandboxTerrainController::FillTerrainRound(const FVector& Origin, float Extend, int MatId) {
	if (!GetWorld()->IsServer()) {
		return;
	}

	struct ZoneHandler : TZoneEditHandler {
		int newMaterialId;
		bool operator()(TVoxelData* vd) {
			changed = false;

			vd->forEachWithCache([&](int x, int y, int z) {
				float density = vd->getDensity(x, y, z);
				FVector o = vd->voxelIndexToVector(x, y, z);
				o += vd->getOrigin();
				o -= Origin;

				float rl = std::sqrt(o.X * o.X + o.Y * o.Y + o.Z * o.Z);
				if (rl < Extend) {
					//2^-((x^2)/20)
					float d = density + 1 / rl * Strength;
					vd->setDensity(x, y, z, d);
					changed = true;
				}

				if (rl < Extend + 20) {
					vd->setMaterial(x, y, z, newMaterialId);
				}
			}, USBT_ENABLE_LOD);

			return changed;
		}
	} Zh;

	Zh.newMaterialId = MatId;
	Zh.Strength = 10;
	Zh.Origin = Origin;
	Zh.Extend = Extend;
	ASandboxTerrainController::PerformTerrainChange(Zh);
}

//======================================================================================================================================================================
// Edit Terrain
//======================================================================================================================================================================

template<class H>
class FTerrainEditThread : public FRunnable {
public:
	H ZoneHandler;
	ASandboxTerrainController* ControllerInstance;

	virtual uint32 Run() {
		ControllerInstance->EditTerrain(ZoneHandler);
		return 0;
	}
};

template<class H>
void ASandboxTerrainController::PerformTerrainChange(H Handler) {
	FTerrainEditThread<H>* EditThread = new FTerrainEditThread<H>();
	EditThread->ZoneHandler = Handler;
	EditThread->ControllerInstance = this;

	FString ThreadName = FString::Printf(TEXT("terrain_change-thread-%d"), FPlatformTime::Seconds());
	FRunnableThread* Thread = FRunnableThread::Create(EditThread, *ThreadName);
	//FIXME delete thread after finish

	FVector TestPoint(Handler.Origin);
	TArray<struct FOverlapResult> Result;
	
	FCollisionQueryParams CollisionQueryParams = FCollisionQueryParams::DefaultQueryParam;
	CollisionQueryParams.bTraceComplex = false;
	CollisionQueryParams.bSkipNarrowPhase = true;

	double Start = FPlatformTime::Seconds();
	bool bIsOverlap = GetWorld()->OverlapMultiByChannel(Result, Handler.Origin, FQuat(), ECC_Visibility, FCollisionShape::MakeSphere(Handler.Extend * 1.5f)); // ECC_Visibility
	double End = FPlatformTime::Seconds();
	double Time = (End - Start) * 1000;
	UE_LOG(LogSandboxTerrain, Log, TEXT("Trace terrain -> %f ms"), Time);

	if (bIsOverlap) {
		for (FOverlapResult& Overlap : Result) {
			if (Cast<ASandboxTerrainController>(Overlap.GetActor())) {
				UHierarchicalInstancedStaticMeshComponent* InstancedMesh = Cast<UHierarchicalInstancedStaticMeshComponent>(Overlap.GetComponent());
				if (InstancedMesh) {
					//UE_LOG(LogSandboxTerrain, Warning, TEXT("InstancedMesh: %s -> %d"), *InstancedMesh->GetName(), Overlap.ItemIndex);
					InstancedMesh->RemoveInstance(Overlap.ItemIndex);

					TArray<USceneComponent*> Parents;
					InstancedMesh->GetParentComponents(Parents);
					if (Parents.Num() > 0) {
						UTerrainZoneComponent* Zone = Cast<UTerrainZoneComponent>(Parents[0]);
						if (Zone) {
							Zone->SetNeedSave(); // TODO check condition racing
						}
					}
				}
			} else {
				//OnOverlapActorDuringTerrainEdit(Overlap, Handler.Origin);
			}
		}
	}
}

template<class H>
void ASandboxTerrainController::PerformZoneEditHandler(TVoxelDataInfoPtr VdInfoPtr, H Handler, std::function<void(TMeshDataPtr)> OnComplete) {
	bool bIsChanged = Handler(VdInfoPtr->Vd);
	if (bIsChanged) {
		VdInfoPtr->SetChanged();
		VdInfoPtr->Vd->setCacheToValid();
		TMeshDataPtr MeshDataPtr = GenerateMesh(VdInfoPtr->Vd);
		VdInfoPtr->ResetLastMeshRegenerationTime();
		OnComplete(MeshDataPtr);
	}
}

// TODO refactor concurency according new terrain data system
template<class H>
void ASandboxTerrainController::EditTerrain(const H& ZoneHandler) {
	double Start = FPlatformTime::Seconds();

	static float ZoneVolumeSize = USBT_ZONE_SIZE / 2;
	TVoxelIndex BaseZoneIndex = GetZoneIndex(ZoneHandler.Origin);

	bool bIsValid = true;

	static const float V[3] = { -1, 0, 1 };
	for (float x : V) {
		for (float y : V) {
			for (float z : V) {
				TVoxelIndex ZoneIndex = BaseZoneIndex + TVoxelIndex(x, y, z);
				UTerrainZoneComponent* Zone = GetZoneByVectorIndex(ZoneIndex);
				TVoxelDataInfoPtr VoxelDataInfo = GetVoxelDataInfo(ZoneIndex);

				// check zone bounds
				FVector ZoneOrigin = GetZonePos(ZoneIndex);
				FVector Upper(ZoneOrigin.X + ZoneVolumeSize, ZoneOrigin.Y + ZoneVolumeSize, ZoneOrigin.Z + ZoneVolumeSize);
				FVector Lower(ZoneOrigin.X - ZoneVolumeSize, ZoneOrigin.Y - ZoneVolumeSize, ZoneOrigin.Z - ZoneVolumeSize);

				if (FMath::SphereAABBIntersection(FSphere(ZoneHandler.Origin, ZoneHandler.Extend * 2.f), FBox(Lower, Upper))) {
					//UE_LOG(LogSandboxTerrain, Log, TEXT("VoxelDataInfo->DataState = %d, Index = %d %d %d"), VoxelDataInfo->DataState, ZoneIndex.X, ZoneIndex.Y, ZoneIndex.Z);
					if (VoxelDataInfo->DataState == TVoxelDataState::UNDEFINED) {
						UE_LOG(LogSandboxTerrain, Warning, TEXT("Zone: %d %d %d -> Invalid zone vd state (UNDEFINED)"), ZoneIndex.X, ZoneIndex.Y, ZoneIndex.Z);
						bIsValid = false;
						break;
					}
				}
			}
		}
	}

	if (!bIsValid) {
		return;
	}

	for (float x : V) {
		for (float y : V) {
			for (float z : V) {
				TVoxelIndex ZoneIndex = BaseZoneIndex + TVoxelIndex(x, y, z);
				UTerrainZoneComponent* Zone = GetZoneByVectorIndex(ZoneIndex);
				TVoxelDataInfoPtr VoxelDataInfo = GetVoxelDataInfo(ZoneIndex);

				// check zone bounds
				FVector ZoneOrigin = GetZonePos(ZoneIndex);
				FVector Upper(ZoneOrigin.X + ZoneVolumeSize, ZoneOrigin.Y + ZoneVolumeSize, ZoneOrigin.Z + ZoneVolumeSize);
				FVector Lower(ZoneOrigin.X - ZoneVolumeSize, ZoneOrigin.Y - ZoneVolumeSize, ZoneOrigin.Z - ZoneVolumeSize);

				if (FMath::SphereAABBIntersection(FSphere(ZoneHandler.Origin, ZoneHandler.Extend * 2.f), FBox(Lower, Upper))) {
					VoxelDataInfo->Lock();

					//UE_LOG(LogSandboxTerrain, Warning, TEXT("Zone: %d %d %d -> %d"), ZoneIndex.X, ZoneIndex.Y, ZoneIndex.Z, (int)VoxelDataInfo->DataState);

					if (VoxelDataInfo->DataState == TVoxelDataState::UNDEFINED) {
						UE_LOG(LogSandboxTerrain, Warning, TEXT("Zone: %d %d %d -> UNDEFINED"), ZoneIndex.X, ZoneIndex.Y, ZoneIndex.Z);
						VoxelDataInfo->Unlock();
						continue;
					}

					if (VoxelDataInfo->DataState == TVoxelDataState::READY_TO_LOAD) {
						TVoxelData* Vd = LoadVoxelDataByIndex(ZoneIndex);
						if (Vd) {
							VoxelDataInfo->Vd = Vd;
							VoxelDataInfo->DataState = TVoxelDataState::LOADED;
						} else {
							VoxelDataInfo->DataState = TVoxelDataState::UNGENERATED;
						}
					}

					if (VoxelDataInfo->DataState == TVoxelDataState::UNGENERATED) {
						VoxelDataInfo->DataState = TVoxelDataState::GENERATION_IN_PROGRESS;

						if (VoxelDataInfo->Vd == nullptr) {
							UE_LOG(LogSandboxTerrain, Warning, TEXT("Zone: %d %d %d -> UNGENERATED"), ZoneIndex.X, ZoneIndex.Y, ZoneIndex.Z);

							TVoxelData* NewVd = NewVoxelData();
							NewVd->setOrigin(GetZonePos(ZoneIndex));
							VoxelDataInfo->Vd = NewVd;
						} else {
							UE_LOG(LogSandboxTerrain, Warning, TEXT("Zone: %d %d %d -> UNGENERATED but Vd is not null"), ZoneIndex.X, ZoneIndex.Y, ZoneIndex.Z);
						}

						GetTerrainGenerator()->ForceGenerateZone(VoxelDataInfo->Vd, ZoneIndex);
						VoxelDataInfo->DataState = TVoxelDataState::GENERATED;
					}

					if (VoxelDataInfo->DataState == TVoxelDataState::LOADED || VoxelDataInfo->DataState == TVoxelDataState::GENERATED) {
						if (Zone == nullptr) {
							PerformZoneEditHandler(VoxelDataInfo, ZoneHandler, [&](TMeshDataPtr MeshDataPtr) {
								TerrainData->PutMeshDataToCache(ZoneIndex, MeshDataPtr);
								ExecGameThreadAddZoneAndApplyMesh(ZoneIndex, MeshDataPtr);
								TerrainData->AddSaveIndex(ZoneIndex);
							});
						} else {
							PerformZoneEditHandler(VoxelDataInfo, ZoneHandler, [&](TMeshDataPtr MeshDataPtr) {
								TerrainData->PutMeshDataToCache(ZoneIndex, MeshDataPtr);
								ExecGameThreadZoneApplyMesh(Zone, MeshDataPtr);
								TerrainData->AddSaveIndex(ZoneIndex);
							});
						}
					}

					TerrainData->AddSaveIndex(ZoneIndex);
					VoxelDataInfo->Unlock();
				}
			}
		}
	}

	double End = FPlatformTime::Seconds();
	double Time = (End - Start) * 1000;
	UE_LOG(LogSandboxTerrain, Log, TEXT("Edit terrain -> %f ms"), Time);
}