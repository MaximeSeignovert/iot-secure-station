#pragma once

#include <Arduino.h>

struct WebAsset {
    const char* contentType = nullptr;
    const char* data = nullptr;
    size_t length = 0;
};

bool webAssetFind(const char* path, WebAsset& out);
