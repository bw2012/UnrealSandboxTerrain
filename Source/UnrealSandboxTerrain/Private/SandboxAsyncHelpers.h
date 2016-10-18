#pragma once

#include "EngineMinimal.h"

struct MeshData;

#define MT_GENERATE_MESH	0
#define MT_ADD_ZONE			1

typedef struct ZoneMakeTask {
	int type;
	FVector index;
	MeshData* mesh_data;
	bool isNew = false;
} ZoneMakeTask;


void sandboxAsyncAddZoneMakeTask(ZoneMakeTask zone_make_task);
ZoneMakeTask sandboxAsyncGetZoneMakeTask();
bool sandboxAsyncIsNextZoneMakeTask();

