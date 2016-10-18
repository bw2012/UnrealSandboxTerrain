#pragma once

#include "EngineMinimal.h"

struct MeshData;


typedef struct ZoneMakeTask {
	FVector index;
	MeshData* mesh_data;
	bool isNew = false;
} ZoneMakeTask;


void sandboxAsyncAddZoneMakeTask(ZoneMakeTask zone_make_task);
ZoneMakeTask sandboxAsyncGetZoneMakeTask();
bool sandboxAsyncIsNextZoneMakeTask();

