// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#define LOD_ARRAY_SIZE				6	//7
#define USBT_ZONE_SIZE				1000.f
//#define USBT_ZONE_DIMENSION			65

#define USBT_VD_UNGENERATED_LOD		2

#define USBT_ENABLE_LOD true

DECLARE_LOG_CATEGORY_EXTERN(LogVt, Log, All);


class FUnrealSandboxTerrainModule : public IModuleInterface {

public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
