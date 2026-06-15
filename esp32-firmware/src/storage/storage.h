#pragma once

#include "../sensors/sensor_data.h"

#include <stddef.h>

typedef bool (*StoragePublishCallback)(const char* payload);

void storageInit();
bool storageEnqueue(const SensorSnapshot& snapshot);
size_t storageFlush(StoragePublishCallback publish);
void storageFsLock();
void storageFsUnlock();
