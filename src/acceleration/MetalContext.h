// Copyright (C) 2024 ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_ACCELERATION_METALCONTEXT_H_
#define SCANTAILOR_ACCELERATION_METALCONTEXT_H_

#ifdef __OBJC__
#import <Metal/Metal.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Check if Metal GPU acceleration is available.
 * @return true if Metal device is available and initialized, false otherwise.
 */
bool metalIsAvailable(void);

/**
 * @brief Get the name of the Metal device (for debugging/display).
 * @return Device name string, or "Not available" if Metal is not available.
 */
const char* metalDeviceName(void);

#ifdef __cplusplus
}
#endif

#ifdef __OBJC__
/**
 * @brief MetalContext singleton - manages Metal device, command queue, and shader library.
 *
 * This class provides centralized access to Metal resources for GPU-accelerated
 * image processing operations. It lazily initializes Metal on first access and
 * provides fallback capability when Metal is not available.
 */
@interface MetalContext : NSObject

+ (instancetype)shared;

@property (nonatomic, readonly, nullable) id<MTLDevice> device;
@property (nonatomic, readonly, nullable) id<MTLCommandQueue> commandQueue;
@property (nonatomic, readonly, nullable) id<MTLLibrary> library;

- (BOOL)isAvailable;
- (void)clearPipelineCache;
- (nullable id<MTLComputePipelineState>)pipelineStateForFunction:(nonnull NSString*)functionName;

@end
#endif

#endif // SCANTAILOR_ACCELERATION_METALCONTEXT_H_
