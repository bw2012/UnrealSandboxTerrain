
#include "SandboxTerrainController.h"


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
		UE_LOG(LogSandboxTerrain, Log, TEXT("create new regular terrain material instance ----> id: %d"), MaterialId);
		UMaterialInstanceDynamic* DynMaterial = UMaterialInstanceDynamic::Create(RegularMaterial, this);

		if (MaterialMap.Contains(MaterialId)) {
			FSandboxTerrainMaterial Mat = MaterialMap[MaterialId];

			if (Mat.TextureTopMicro) {
				DynMaterial->SetTextureParameterValue("TextureTopMicro", Mat.TextureTopMicro);
				DynMaterial->SetTextureParameterValue("TextureSideMicro", Mat.TextureSideMicro);
			}
			else {
				DynMaterial->SetTextureParameterValue("TextureTopMicro", Mat.TextureSideMicro);
				DynMaterial->SetTextureParameterValue("TextureSideMicro", Mat.TextureSideMicro);
			}

			if (Mat.TextureTopMacro) {
				DynMaterial->SetTextureParameterValue("TextureTopMacro", Mat.TextureTopMacro);
				DynMaterial->SetTextureParameterValue("TextureSideMacro", Mat.TextureSideMacro);
			}
			else {
				DynMaterial->SetTextureParameterValue("TextureTopMacro", Mat.TextureSideMacro);
				DynMaterial->SetTextureParameterValue("TextureSideMacro", Mat.TextureSideMacro);
			}

			if (Mat.TextureTopNormal) {
				DynMaterial->SetTextureParameterValue("TextureTopNormal", Mat.TextureTopNormal);
				DynMaterial->SetTextureParameterValue("TextureSideNormal", Mat.TextureSideNormal);
			}
			else {
				DynMaterial->SetTextureParameterValue("TextureSideNormal", Mat.TextureSideNormal);
				DynMaterial->SetTextureParameterValue("TextureSideNormal", Mat.TextureSideNormal);
			}
		}

		RegularMaterialCache.Add(MaterialId, DynMaterial);
		return DynMaterial;
	}

	return RegularMaterialCache[MaterialId];
}

UMaterialInterface* ASandboxTerrainController::GetTransitionTerrainMaterial(const std::set<unsigned short>& MaterialIdSet) {
	if (TransitionMaterial == nullptr) {
		return nullptr;
	}

	uint64 Code = TMeshMaterialTransitionSection::GenerateTransitionCode(MaterialIdSet);
	if (!TransitionMaterialCache.Contains(Code)) {
		TTransitionMaterialCode tmp;
		tmp.Code = Code;

		UE_LOG(LogSandboxTerrain, Log, TEXT("create new transition terrain material instance ----> id: %llu (%lu-%lu-%lu)"), Code, tmp.TriangleMatId[0], tmp.TriangleMatId[1], tmp.TriangleMatId[2]);
		UMaterialInstanceDynamic* DynMaterial = UMaterialInstanceDynamic::Create(TransitionMaterial, this);

		int Idx = 0;
		for (unsigned short MatId : MaterialIdSet) {
			if (MaterialMap.Contains(MatId)) {
				FSandboxTerrainMaterial Mat = MaterialMap[MatId];

				FName TextureTopMicroParam = FName(*FString::Printf(TEXT("TextureTopMicro%d"), Idx));
				FName TextureSideMicroParam = FName(*FString::Printf(TEXT("TextureSideMicro%d"), Idx));
				FName TextureTopNormalParam = FName(*FString::Printf(TEXT("TextureTopNormal%d"), Idx));
				FName TextureSideNormalParam = FName(*FString::Printf(TEXT("TextureSideNormal%d"), Idx));

				if (Mat.TextureTopMicro) {
					DynMaterial->SetTextureParameterValue(TextureTopMicroParam, Mat.TextureTopMicro);
					DynMaterial->SetTextureParameterValue(TextureSideMicroParam, Mat.TextureSideMicro);
				}
				else {
					DynMaterial->SetTextureParameterValue(TextureTopMicroParam, Mat.TextureSideMicro);
					DynMaterial->SetTextureParameterValue(TextureSideMicroParam, Mat.TextureSideMicro);
				}

				if (Mat.TextureTopNormal) {
					DynMaterial->SetTextureParameterValue(TextureTopNormalParam, Mat.TextureTopNormal);
					DynMaterial->SetTextureParameterValue(TextureSideNormalParam, Mat.TextureSideNormal);
				}
				else {
					DynMaterial->SetTextureParameterValue(TextureTopNormalParam, Mat.TextureSideNormal);
					DynMaterial->SetTextureParameterValue(TextureSideNormalParam, Mat.TextureSideNormal);
				}
			}

			Idx++;
		}

		TransitionMaterialCache.Add(Code, DynMaterial);
		return DynMaterial;
	}

	return TransitionMaterialCache[Code];
}