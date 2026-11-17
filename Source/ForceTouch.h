#pragma once
#include <functional>

// Force Touch trackpads (macOS): reports how hard the trackpad is pressed, while it is pressed.
// pressure: 0..1 within the current stage; stage: 0 released, 1 click, 2 deep click.
// Elsewhere these do nothing.
namespace orbitforce {
#if defined(__APPLE__)
void* start(std::function<void(float pressure, int stage)> callback);
void stop(void* monitor);
#else
inline void* start(std::function<void(float, int)>) { return nullptr; }
inline void stop(void*) {}
#endif
}
