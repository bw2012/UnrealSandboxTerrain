
#include "UnrealSandboxTerrainPrivatePCH.h"
#include "SandboxAsyncHelpers.h"
#include <queue>
#include <mutex>

/*
std::mutex zone_make_queue_mutex;
std::queue<TerrainControllerTask> zone_make_queue;

void sandboxAsyncAddTask(TerrainControllerTask zone_make_task) {
	zone_make_queue_mutex.lock();
	zone_make_queue.push(zone_make_task);
	zone_make_queue_mutex.unlock();
}

TerrainControllerTask sandboxAsyncGetTask() {
	zone_make_queue_mutex.lock();
	TerrainControllerTask zone_make_task = zone_make_queue.front();
	zone_make_queue.pop();
	zone_make_queue_mutex.unlock();

	return zone_make_task;
}

bool sandboxAsyncIsNextTask() {
	return zone_make_queue.size() > 0;
}
*/