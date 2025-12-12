// Copyright (C) 2024 ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#import "MetalContext.h"
#import <Foundation/Foundation.h>

static NSString* const kDefaultLibraryName = @"default";

@implementation MetalContext {
    NSMutableDictionary<NSString*, id<MTLComputePipelineState>>* _pipelineCache;
    NSString* _deviceName;
    dispatch_queue_t _syncQueue;  // Serial queue for thread-safe access
}

+ (instancetype)shared {
    static MetalContext* instance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        instance = [[MetalContext alloc] init];
    });
    return instance;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        _pipelineCache = [NSMutableDictionary dictionary];
        _deviceName = @"Not available";
        _syncQueue = dispatch_queue_create("com.scantailor.metalcontext", DISPATCH_QUEUE_SERIAL);

        // Get default Metal device
        _device = MTLCreateSystemDefaultDevice();
        if (!_device) {
            NSLog(@"MetalContext: No Metal device available");
            return self;
        }

        _deviceName = _device.name;
        NSLog(@"MetalContext: Using Metal device: %@", _deviceName);

        // Create command queue
        _commandQueue = [_device newCommandQueue];
        if (!_commandQueue) {
            NSLog(@"MetalContext: Failed to create command queue");
            _device = nil;
            return self;
        }

        // Load compiled shader library from app bundle
        NSBundle* mainBundle = [NSBundle mainBundle];
        NSString* libraryPath = [mainBundle pathForResource:kDefaultLibraryName ofType:@"metallib"];

        if (libraryPath) {
            NSError* error = nil;
            _library = [_device newLibraryWithFile:libraryPath error:&error];
            if (error) {
                NSLog(@"MetalContext: Failed to load metal library from %@: %@", libraryPath, error);
                _library = nil;
            } else {
                NSLog(@"MetalContext: Loaded shader library from %@", libraryPath);
            }
        } else {
            // Try loading default library (for development - shaders compiled into app)
            NSError* error = nil;
            _library = [_device newDefaultLibrary];
            if (!_library) {
                NSLog(@"MetalContext: No shader library found in bundle");
            } else {
                NSLog(@"MetalContext: Using default shader library");
            }
        }
    }
    return self;
}

- (BOOL)isAvailable {
    return _device != nil && _commandQueue != nil && _library != nil;
}

- (void)clearPipelineCache {
    dispatch_sync(_syncQueue, ^{
        [_pipelineCache removeAllObjects];
    });
}

- (nullable id<MTLComputePipelineState>)pipelineStateForFunction:(NSString*)functionName {
    if (![self isAvailable]) {
        return nil;
    }

    __block id<MTLComputePipelineState> result = nil;

    dispatch_sync(_syncQueue, ^{
        // Check cache first
        id<MTLComputePipelineState> cached = _pipelineCache[functionName];
        if (cached) {
            result = cached;
            return;
        }

        // Create new pipeline state
        id<MTLFunction> function = [_library newFunctionWithName:functionName];
        if (!function) {
            NSLog(@"MetalContext: Function '%@' not found in shader library", functionName);
            return;
        }

        NSError* error = nil;
        id<MTLComputePipelineState> pipelineState = [_device newComputePipelineStateWithFunction:function error:&error];
        if (error) {
            NSLog(@"MetalContext: Failed to create pipeline state for '%@': %@", functionName, error);
            return;
        }

        // Cache for reuse
        _pipelineCache[functionName] = pipelineState;
        result = pipelineState;
    });

    return result;
}

@end

// C interface implementations
bool metalIsAvailable(void) {
    return [[MetalContext shared] isAvailable];
}

const char* metalDeviceName(void) {
    static char deviceNameBuffer[256] = {0};
    MetalContext* ctx = [MetalContext shared];
    if (ctx.device) {
        [ctx.device.name getCString:deviceNameBuffer maxLength:255 encoding:NSUTF8StringEncoding];
    } else {
        strcpy(deviceNameBuffer, "Not available");
    }
    return deviceNameBuffer;
}
