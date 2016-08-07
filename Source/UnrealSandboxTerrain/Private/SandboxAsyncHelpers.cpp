
#include "UnrealSandboxTerrainPrivatePCH.h"
#include "SandboxAsyncHelpers.h"
#include <queue>
#include <mutex>

std::mutex zone_make_queue_mutex;
std::queue<ZoneMakeTask> zone_make_queue;

void sandboxAsyncAddZoneMakeTask(ZoneMakeTask zone_make_task) {
	zone_make_queue_mutex.lock();
	zone_make_queue.push(zone_make_task);
	zone_make_queue_mutex.unlock();
}

ZoneMakeTask sandboxAsyncGetZoneMakeTask() {
	zone_make_queue_mutex.lock();
	ZoneMakeTask zone_make_task = zone_make_queue.front();
	zone_make_queue.pop();
	zone_make_queue_mutex.unlock();

	return zone_make_task;
}

bool sandboxAsyncIsNextZoneMakeTask() {
	return zone_make_queue.size() > 0;
}
