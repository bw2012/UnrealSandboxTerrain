
#include "SandboxTerrainController.h"
#include "VoxelMeshData.h"

//======================================================================================================================================================================
// Materials
//======================================================================================================================================================================

bool ASandboxTerrainController::GetTerrainMaterialInfoById(uint16 MaterialId, FSandboxTerrainMaterial& MaterialInfo) {
	if (MaterialMap.Contains(MaterialId)) {
		MaterialInfo = MaterialMap[MaterialId];
		return true;
	}

	return false;
}

UMaterialInterface* ASandboxTerrainController::GetRegularTerrainMaterial(uint16 MaterialId) {
	if (RegularMaterial == nullptr) {
		return nullptr;
	}

	if (!RegularMaterialCache.Contains(MaterialId)) {
		UE_LOG(LogVt, Log, TEXT("create new regular terrain material instance -> id: %d"), MaterialId);
		UMaterialInstanceDynamic* DynMaterial = UMaterialInstanceDynamic::Create(RegularMaterial, this);

		if (MaterialMap.Contains(MaterialId)) {
			FSandboxTerrainMaterial Mat = MaterialMap[MaterialId];
			DynMaterial->SetTextureParameterValue("TextureDiffuse", Mat.TextureDiffuse);
			DynMaterial->SetTextureParameterValue("TextureNormal", Mat.TextureNormal);
		}

		RegularMaterialCache.Add(MaterialId, DynMaterial);
		return DynMaterial;
	}

	return RegularMaterialCache[MaterialId];
}

UMaterialInterface* ASandboxTerrainController::GetTransitionMaterial(const std::set<unsigned short>& MaterialIdSet) {
	if (TransitionMaterial == nullptr) {
		return nullptr;
	}

	uint64 Code = TMeshMaterialTransitionSection::GenerateTransitionCode(MaterialIdSet);
	if (!TransitionMaterialCache.Contains(Code)) {
		TTransitionMaterialCode tmp;
		tmp.Code = Code;

		UE_LOG(LogVt, Log, TEXT("create new transition terrain material instance -> id: %llu (%lu-%lu-%lu)"), Code, tmp.TriangleMatId[0], tmp.TriangleMatId[1], tmp.TriangleMatId[2]);
		UMaterialInstanceDynamic* DynMaterial = UMaterialInstanceDynamic::Create(TransitionMaterial, this);

		int Idx = 0;
		for (unsigned short MatId : MaterialIdSet) {
			if (MaterialMap.Contains(MatId)) {
				FSandboxTerrainMaterial Mat = MaterialMap[MatId];
				FName TextureDiffuseParam = FName(*FString::Printf(TEXT("TextureDiffuse%d"), Idx));
				FName TextureNormalParam = FName(*FString::Printf(TEXT("TextureNormal%d"), Idx));
				DynMaterial->SetTextureParameterValue(TextureDiffuseParam, Mat.TextureDiffuse);
				DynMaterial->SetTextureParameterValue(TextureNormalParam, Mat.TextureNormal);
			}

			Idx++;
		}

		TransitionMaterialCache.Add(Code, DynMaterial);
		return DynMaterial;
	}

	return TransitionMaterialCache[Code];
}