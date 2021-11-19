// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"

#define USBT_ZONE_SIZE				1000.f
#define USBT_ZONE_DIMENSION			65

#define USBT_VD_UNGENERATED_LOD		2

#define USBT_REGION_SIZE			9000.f

#define USBT_REGION_FILE_VERSION		1
#define USBT_REGION_VOXELDATA_VERSION	1


#define USBT_ENABLE_LOD true


//======================================================================
// voxel data network operation codes
//======================================================================

// client request 
#define USBT_NET_OPCODE_ASK_HELLO			0x0001
#define USBT_NET_OPCODE_ASK_ALL_VD			0x0002

// server response 
#define USBT_NET_OPCODE_RESPONSE_VERSION	0x0100
#define USBT_NET_OPCODE_RESPONSE_VD			0x0200

// common 
#define USBT_NET_OPCODE_DIG_ROUND			0x000A

//======================================================================

DECLARE_LOG_CATEGORY_EXTERN(LogSandboxTerrain, Log, All);

class FUnrealSandboxTerrainModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

};
