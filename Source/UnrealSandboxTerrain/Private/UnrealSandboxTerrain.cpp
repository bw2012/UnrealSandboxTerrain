// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealSandboxTerrainPrivatePCH.h"

#define LOCTEXT_NAMESPACE "FUnrealSandboxTerrainModule"

DEFINE_LOG_CATEGORY(LogSandboxTerrain);

void FUnrealSandboxTerrainModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	UE_LOG(LogSandboxTerrain, Log, TEXT("------------------ UnrealSandboxTerrain plugin initialize ------------------"));
}

void FUnrealSandboxTerrainModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUnrealSandboxTerrainModule, UnrealSandboxTerrain)