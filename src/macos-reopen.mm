#include "activation.h"
#import <Cocoa/Cocoa.h>

namespace { std::function<void()> reopenCallback; }
@interface PetDeskReopenReceiver : NSObject
- (void)handleReopen:(NSAppleEventDescriptor *)event reply:(NSAppleEventDescriptor *)reply;
@end
@implementation PetDeskReopenReceiver
- (void)handleReopen:(NSAppleEventDescriptor *)event reply:(NSAppleEventDescriptor *)reply {
    (void)event; (void)reply;
    if (reopenCallback) reopenCallback();
}
@end
namespace {
PetDeskReopenReceiver *receiver() {
    static PetDeskReopenReceiver *value = [[PetDeskReopenReceiver alloc] init];
    return value;
}
void installReopenReceiver() {
    [[NSAppleEventManager sharedAppleEventManager] setEventHandler:receiver()
        andSelector:@selector(handleReopen:reply:) forEventClass:kCoreEventClass andEventID:kAEReopenApplication];
}
id launchObserver;
}
namespace petdesk {
void setReopenHandler(std::function<void()> callback) {
    reopenCallback = std::move(callback);
    if (reopenCallback) {
        installReopenReceiver();
        // AppKit's launch setup may replace the initial AppleEvent handler.
        if (!launchObserver) launchObserver = [[NSNotificationCenter defaultCenter]
            addObserverForName:NSApplicationDidFinishLaunchingNotification object:nil queue:nil
            usingBlock:^(NSNotification *) { if (reopenCallback) installReopenReceiver(); }];
    } else {
        [[NSAppleEventManager sharedAppleEventManager] removeEventHandlerForEventClass:kCoreEventClass andEventID:kAEReopenApplication];
        if (launchObserver) { [[NSNotificationCenter defaultCenter] removeObserver:launchObserver]; launchObserver = nil; }
    }
}
}
