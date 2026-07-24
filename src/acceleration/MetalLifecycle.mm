// Copyright (C) 2024 ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#import "MetalLifecycle.h"
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <atomic>

// Atomic flag for lock-free checking in hot path
static std::atomic<bool> g_appActive{true};
static std::atomic<bool> g_initialized{false};

@interface MetalLifecycleObserver : NSObject
@end

@implementation MetalLifecycleObserver

- (instancetype)init {
    self = [super init];
    if (self) {
        NSNotificationCenter* center = [NSNotificationCenter defaultCenter];

        // App became active (returned to foreground)
        [center addObserver:self
                   selector:@selector(appDidBecomeActive:)
                       name:NSApplicationDidBecomeActiveNotification
                     object:nil];

        // App resigned active (going to background)
        [center addObserver:self
                   selector:@selector(appWillResignActive:)
                       name:NSApplicationWillResignActiveNotification
                     object:nil];

        // Also listen for termination to clean up
        [center addObserver:self
                   selector:@selector(appWillTerminate:)
                       name:NSApplicationWillTerminateNotification
                     object:nil];

        // Set initial state based on current app state
        g_appActive.store([NSApp isActive], std::memory_order_release);
    }
    return self;
}

- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)appDidBecomeActive:(NSNotification*)notification {
    g_appActive.store(true, std::memory_order_release);
}

- (void)appWillResignActive:(NSNotification*)notification {
    g_appActive.store(false, std::memory_order_release);
}

- (void)appWillTerminate:(NSNotification*)notification {
    g_appActive.store(false, std::memory_order_release);
}

@end

// Static instance to keep observer alive
static MetalLifecycleObserver* g_observer = nil;

void metalLifecycleInit(void) {
    // Use atomic compare-exchange for thread-safe one-time initialization
    bool expected = false;
    if (g_initialized.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        // Must dispatch to main thread since NSNotificationCenter observers
        // for app lifecycle events need to be on main thread
        if ([NSThread isMainThread]) {
            g_observer = [[MetalLifecycleObserver alloc] init];
        } else {
            dispatch_sync(dispatch_get_main_queue(), ^{
                g_observer = [[MetalLifecycleObserver alloc] init];
            });
        }
    }
}

bool metalIsAppActive(void) {
    return g_appActive.load(std::memory_order_acquire);
}
