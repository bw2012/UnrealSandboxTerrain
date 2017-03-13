// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"

#define USBT_REGION_FILE_VERSION		1
#define USBT_REGION_VOXELDATA_VERSION	1

DECLARE_LOG_CATEGORY_EXTERN(LogSandboxTerrain, Log, All);

class FUnrealSandboxTerrainModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

};

