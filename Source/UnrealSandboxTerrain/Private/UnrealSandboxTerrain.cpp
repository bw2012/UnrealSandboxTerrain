// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealSandboxTerrain.h"

#define LOCTEXT_NAMESPACE "FUnrealSandboxTerrainModule"


float LodScreenSizeArray[LOD_ARRAY_SIZE];

TAutoConsoleVariable<int32> CVarMainDistance (
	TEXT("vt.MainDistance"),
	-1,
	TEXT("Override voxel terrain view/generation distance (zones)\n")
	TEXT(" -1 = Off. Use terrain actor settings \n")
	TEXT(" 0...30 = View and terrain stream distance \n"),
	ECVF_Scalability);

void FUnrealSandboxTerrainModule::StartupModule() {
	float LodRatio = 2.f;
	float ScreenSize = 1.f;
	for (auto LodIdx = 0; LodIdx < LOD_ARRAY_SIZE; LodIdx++) {
		LodScreenSizeArray[LodIdx] = ScreenSize;
		ScreenSize /= LodRatio;
	}

}

void FUnrealSandboxTerrainModule::ShutdownModule() {

	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUnrealSandboxTerrainModule, UnrealSandboxTerrain)

DEFINE_LOG_CATEGORY(LogSandboxTerrain);