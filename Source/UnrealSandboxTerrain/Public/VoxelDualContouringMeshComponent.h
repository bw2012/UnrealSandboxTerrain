// Copyright blackw 2015-2020

#pragma once

#include "Engine.h"
#include "VoxelMeshComponent.h"
#include "VoxelDualContouringMeshComponent.generated.h"




/**
*
*/
UCLASS( ClassGroup = (Custom), meta = (BlueprintSpawnableComponent) )
class UNREALSANDBOXTERRAIN_API UVoxelDualContouringMeshComponent : public UVoxelMeshComponent {
	GENERATED_UCLASS_BODY()


public:

	//virtual void BeginDestroy();

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);

	UPROPERTY(EditAnywhere)
	UMaterial* BasicMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Mesh Generation")
	float VoxelVolumeSize;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Mesh Generation")
	int VoxelSize;

	void EditMeshDeleteSphere(const FVector& Origin, float Radius, float Strength);

protected:
	TVoxelData* VoxelData;

	void MakeMesh();

};