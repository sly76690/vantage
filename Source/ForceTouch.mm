#import <AppKit/AppKit.h>
#include <memory>
#include "ForceTouch.h"

namespace orbitforce {
void* start(std::function<void(float, int)> callback) {
 auto handler = std::make_shared<std::function<void(float, int)>>(std::move(callback));
 id monitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskPressure handler:^NSEvent*(NSEvent* event) {
  (*handler)((float) event.pressure, (int) event.stage);
  return event; // the event goes on to the application as usual
 }];
#if __has_feature(objc_arc)
 return (__bridge_retained void*) monitor;
#else
 return (void*) [monitor retain];
#endif
}

void stop(void* monitor) {
 if (monitor == nullptr) return;
#if __has_feature(objc_arc)
 id object = (__bridge_transfer id) monitor;
 [NSEvent removeMonitor:object];
#else
 id object = (id) monitor;
 [NSEvent removeMonitor:object];
 [object release];
#endif
}
}
