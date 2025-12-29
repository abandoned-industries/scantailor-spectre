// Copyright (C) 2024 ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#import "MetalMorphology.h"
#import "MetalContext.h"
#import "MetalLifecycle.h"
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

// Minimum image size to use GPU (smaller images are faster on CPU)
// On Apple Silicon with unified memory, GPU overhead is minimal, allowing
// acceleration for smaller images.
static const int MIN_GPU_SIZE = 64;

// Serial queue for thread-safe Metal operations
static dispatch_queue_t getMorphologyQueue(void) {
    static dispatch_queue_t queue = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        queue = dispatch_queue_create("com.scantailor.metalmorphology", DISPATCH_QUEUE_SERIAL);
    });
    return queue;
}

bool metalMorphologyAvailable(void) {
    // Check both: Metal context available AND app is in foreground
    // Skip GPU when backgrounded to avoid crashes from purged GPU resources
    return metalIsAvailable() && metalIsAppActive();
}

// Helper to perform a two-pass morphological operation (horizontal then vertical)
static bool performMorphOp(uint8_t* data, int width, int height, int stride,
                           int brickWidth, int brickHeight, uint8_t borderValue,
                           NSString* hKernelName, NSString* vKernelName) {
    // Skip GPU for small images
    if (width < MIN_GPU_SIZE || height < MIN_GPU_SIZE) {
        return false;
    }

    MetalContext* ctx = [MetalContext shared];
    if (![ctx isAvailable]) {
        return false;
    }

    // Serialize all Metal operations through dispatch queue with autoreleasepool
    __block bool success = false;
    dispatch_sync(getMorphologyQueue(), ^{
        @autoreleasepool {
            id<MTLDevice> device = ctx.device;
            id<MTLCommandQueue> queue = ctx.commandQueue;

            // Get pipeline states for horizontal and vertical passes
            id<MTLComputePipelineState> hPipeline = [ctx pipelineStateForFunction:hKernelName];
            id<MTLComputePipelineState> vPipeline = [ctx pipelineStateForFunction:vKernelName];

            if (!hPipeline || !vPipeline) {
                return;
            }

            // Create texture descriptor for R8Unorm (grayscale)
            MTLTextureDescriptor* texDesc = [[MTLTextureDescriptor alloc] init];
            texDesc.textureType = MTLTextureType2D;
            texDesc.pixelFormat = MTLPixelFormatR8Unorm;
            texDesc.width = width;
            texDesc.height = height;
            texDesc.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
            texDesc.storageMode = MTLStorageModeShared;

            // Create textures: input, temp (for between passes), output
            id<MTLTexture> inputTexture = [device newTextureWithDescriptor:texDesc];
            id<MTLTexture> tempTexture = [device newTextureWithDescriptor:texDesc];
            id<MTLTexture> outputTexture = [device newTextureWithDescriptor:texDesc];

            if (!inputTexture || !tempTexture || !outputTexture) {
                return;
            }

            // Copy input data to texture
            MTLRegion region = MTLRegionMake2D(0, 0, width, height);
            [inputTexture replaceRegion:region mipmapLevel:0 withBytes:data bytesPerRow:stride];

            // Calculate radii (brick size to radius conversion)
            int hRadius = brickWidth / 2;
            int vRadius = brickHeight / 2;

            // Border value as float (0.0 to 1.0)
            float borderFloat = (float)borderValue / 255.0f;

            // Create command buffer
            id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
            if (!commandBuffer) {
                return;
            }

            // Horizontal pass (input -> temp)
            if (brickWidth > 1) {
                id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
                if (!encoder) {
                    return;
                }

                [encoder setComputePipelineState:hPipeline];
                [encoder setTexture:inputTexture atIndex:0];
                [encoder setTexture:tempTexture atIndex:1];
                [encoder setBytes:&hRadius length:sizeof(int) atIndex:0];
                [encoder setBytes:&borderFloat length:sizeof(float) atIndex:1];

                MTLSize gridSize = MTLSizeMake(width, height, 1);
                MTLSize threadGroupSize = MTLSizeMake(16, 16, 1);
                threadGroupSize.width = MIN(threadGroupSize.width, hPipeline.maxTotalThreadsPerThreadgroup);
                threadGroupSize.height = MIN(threadGroupSize.height, hPipeline.maxTotalThreadsPerThreadgroup / threadGroupSize.width);

                [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadGroupSize];
                [encoder endEncoding];
            } else {
                // No horizontal pass needed, just copy
                id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];
                [blitEncoder copyFromTexture:inputTexture toTexture:tempTexture];
                [blitEncoder endEncoding];
            }

            // Vertical pass (temp -> output)
            if (brickHeight > 1) {
                id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
                if (!encoder) {
                    return;
                }

                [encoder setComputePipelineState:vPipeline];
                [encoder setTexture:tempTexture atIndex:0];
                [encoder setTexture:outputTexture atIndex:1];
                [encoder setBytes:&vRadius length:sizeof(int) atIndex:0];
                [encoder setBytes:&borderFloat length:sizeof(float) atIndex:1];

                MTLSize gridSize = MTLSizeMake(width, height, 1);
                MTLSize threadGroupSize = MTLSizeMake(16, 16, 1);
                threadGroupSize.width = MIN(threadGroupSize.width, vPipeline.maxTotalThreadsPerThreadgroup);
                threadGroupSize.height = MIN(threadGroupSize.height, vPipeline.maxTotalThreadsPerThreadgroup / threadGroupSize.width);

                [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadGroupSize];
                [encoder endEncoding];
            } else {
                // No vertical pass needed, just copy
                id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];
                [blitEncoder copyFromTexture:tempTexture toTexture:outputTexture];
                [blitEncoder endEncoding];
            }

            // Execute and wait
            [commandBuffer commit];
            [commandBuffer waitUntilCompleted];

            if (commandBuffer.status != MTLCommandBufferStatusCompleted) {
                return;
            }

            // Copy result back to CPU memory
            [outputTexture getBytes:data bytesPerRow:stride fromRegion:region mipmapLevel:0];

            success = true;
        }
    });

    return success;
}

bool metalErodeGray(uint8_t* data, int width, int height, int stride,
                    int brickWidth, int brickHeight, uint8_t borderValue) {
    return performMorphOp(data, width, height, stride,
                          brickWidth, brickHeight, borderValue,
                          @"erodeHorizontal", @"erodeVertical");
}

bool metalDilateGray(uint8_t* data, int width, int height, int stride,
                     int brickWidth, int brickHeight, uint8_t borderValue) {
    return performMorphOp(data, width, height, stride,
                          brickWidth, brickHeight, borderValue,
                          @"dilateHorizontal", @"dilateVertical");
}
