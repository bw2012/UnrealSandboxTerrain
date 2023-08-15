// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealSandboxTerrain.h"

#define LOCTEXT_NAMESPACE "FUnrealSandboxTerrainModule"


float LodScreenSizeArray[LOD_ARRAY_SIZE];

TAutoConsoleVariable<int32> CVarMainDistance (
	TEXT("vt.MainDistance"),
	-1,
	TEXT("Override voxel terrain view/generation distance (zones) \n")
	TEXT(" -1 = Off. Use terrain actor settings \n")
	TEXT(" 0...30 = View and terrain stream distance \n"),
	ECVF_Scalability);


TAutoConsoleVariable<int32> CVarDebugArea (
	TEXT("vt.DebugArea"),
	0,
	TEXT("Load terrain only in debug area \n")
	TEXT(" 0 = Off \n")
	TEXT(" 1 = 1x1 \n")
	TEXT(" 2 = 3x3 \n"),
	ECVF_Default);


TAutoConsoleVariable<int32> CVarGeneratorDebugMode (
	TEXT("vt.GeneratorDebugMode"),
	0,
	TEXT("Terrain generator debug modes \n")
	TEXT(" 0 = Off \n")
	TEXT(" 1 = Trace generation time \n")
	TEXT(" 2 = Trace generation time and disable fast generation \n"),
	ECVF_Default);


TAutoConsoleVariable<int32> CVarAutoSavePeriod (
	TEXT("vt.AutoSave"),
	-1,
	TEXT("Voxel terrain auto save period \n")
	TEXT(" 0 = Off. Use actor settings \n")
	TEXT(" 0...100 = auto save period in seconds \n"),
	ECVF_SetBySystemSettingsIni);


TAutoConsoleVariable<int32> CVarLodRatio (
	TEXT("vt.TerrainLodRatio"),
	-1,
	TEXT("Overrides voxel terrain zones lod factor \n")
	TEXT(" 0 = Off. Use actor settings \n")
	TEXT(" 0...1 = lod ratio \n"),
	ECVF_SetBySystemSettingsIni);



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

DEFINE_LOG_CATEGORY(LogVt);