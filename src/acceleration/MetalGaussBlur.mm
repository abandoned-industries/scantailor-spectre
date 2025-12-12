// Copyright (C) 2024 ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#import "MetalGaussBlur.h"
#import "MetalContext.h"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <simd/simd.h>
#import <cmath>
#import <vector>

// Minimum image dimension to use GPU (below this, CPU is faster due to overhead)
// On Apple Silicon with unified memory, GPU overhead is very low, so we can use
// GPU acceleration for smaller images than on discrete GPU systems.
static const int MIN_GPU_DIMENSION = 64;

// Minimum sigma to apply blur (below this, blur is imperceptible)
static const float MIN_SIGMA = 0.5f;

// Maximum sigma supported (larger values need bigger kernels)
static const float MAX_SIGMA = 100.0f;

/**
 * Compute Gaussian kernel weights for given sigma.
 * Returns weights for center and positive offsets only (kernel is symmetric).
 * Weights are normalized to sum to 1.0.
 */
static std::vector<float> computeGaussianWeights(float sigma) {
    // Kernel radius: 3 sigma captures 99.7% of the distribution
    int radius = static_cast<int>(std::ceil(sigma * 3.0f));
    if (radius < 1) radius = 1;

    std::vector<float> weights(radius + 1);
    float sum = 0.0f;
    float twoSigmaSq = 2.0f * sigma * sigma;

    // Center weight
    weights[0] = 1.0f;
    sum = weights[0];

    // Offset weights (symmetric, so count each twice in sum)
    for (int i = 1; i <= radius; i++) {
        float w = std::exp(-static_cast<float>(i * i) / twoSigmaSq);
        weights[i] = w;
        sum += 2.0f * w;  // Each offset appears twice (left and right)
    }

    // Normalize
    for (int i = 0; i <= radius; i++) {
        weights[i] /= sum;
    }

    return weights;
}

bool metalGaussBlurAvailable(void) {
    // Disabled due to crashes when app is backgrounded and GPU resources are purged.
    // CPU fallback is fast enough for most use cases.
    return false;
    // return [[MetalContext shared] isAvailable];
}

// Serial queue for thread-safe Metal operations
static dispatch_queue_t getMetalQueue(void) {
    static dispatch_queue_t queue = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        queue = dispatch_queue_create("com.scantailor.metalblur", DISPATCH_QUEUE_SERIAL);
    });
    return queue;
}

bool metalGaussBlur(uint8_t* data, int width, int height, int stride,
                    float hSigma, float vSigma) {
    // Validate inputs
    if (!data || width <= 0 || height <= 0 || stride < width) {
        return false;
    }

    // Check if blur is meaningful
    if (hSigma < MIN_SIGMA && vSigma < MIN_SIGMA) {
        return true;  // No blur needed, consider it "done"
    }

    // Clamp sigma values
    hSigma = std::min(hSigma, MAX_SIGMA);
    vSigma = std::min(vSigma, MAX_SIGMA);

    // Check if image is large enough for GPU to be worthwhile
    if (width < MIN_GPU_DIMENSION && height < MIN_GPU_DIMENSION) {
        return false;  // Use CPU fallback
    }

    // Get Metal context
    MetalContext* ctx = [MetalContext shared];
    if (![ctx isAvailable]) {
        return false;
    }

    // Serialize all Metal operations to avoid driver-level race conditions
    __block bool result = false;
    dispatch_sync(getMetalQueue(), ^{
    @autoreleasepool {
        id<MTLDevice> device = ctx.device;
        id<MTLCommandQueue> commandQueue = ctx.commandQueue;

        // Get pipeline states for our kernels
        id<MTLComputePipelineState> hBlurPipeline = [ctx pipelineStateForFunction:@"gaussBlurHorizontal"];
        id<MTLComputePipelineState> vBlurPipeline = [ctx pipelineStateForFunction:@"gaussBlurVertical"];

        if (!hBlurPipeline || !vBlurPipeline) {
            NSLog(@"MetalGaussBlur: Failed to get pipeline states");
            result = false;
            return;
        }

        // Create texture descriptor for grayscale images
        MTLTextureDescriptor* texDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR32Float
                                                                                            width:width
                                                                                           height:height
                                                                                        mipmapped:NO];
        texDesc.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
        texDesc.storageMode = MTLStorageModeShared;  // Unified memory - no copy needed

        // Create textures
        id<MTLTexture> inputTexture = [device newTextureWithDescriptor:texDesc];
        id<MTLTexture> tempTexture = [device newTextureWithDescriptor:texDesc];
        id<MTLTexture> outputTexture = [device newTextureWithDescriptor:texDesc];

        if (!inputTexture || !tempTexture || !outputTexture) {
            NSLog(@"MetalGaussBlur: Failed to create textures");
            result = false;
            return;
        }

        // Copy input data to texture (convert uint8 -> float)
        {
            std::vector<float> floatData(width * height);
            for (int y = 0; y < height; y++) {
                const uint8_t* row = data + y * stride;
                for (int x = 0; x < width; x++) {
                    floatData[y * width + x] = row[x] / 255.0f;
                }
            }
            MTLRegion region = MTLRegionMake2D(0, 0, width, height);
            [inputTexture replaceRegion:region mipmapLevel:0 withBytes:floatData.data() bytesPerRow:width * sizeof(float)];
        }

        // Compute Gaussian weights
        std::vector<float> hWeights = (hSigma >= MIN_SIGMA) ? computeGaussianWeights(hSigma) : std::vector<float>{1.0f};
        std::vector<float> vWeights = (vSigma >= MIN_SIGMA) ? computeGaussianWeights(vSigma) : std::vector<float>{1.0f};

        int hRadius = static_cast<int>(hWeights.size() - 1);
        int vRadius = static_cast<int>(vWeights.size() - 1);

        // Create buffers for weights
        id<MTLBuffer> hWeightsBuffer = [device newBufferWithBytes:hWeights.data()
                                                          length:hWeights.size() * sizeof(float)
                                                         options:MTLResourceStorageModeShared];
        id<MTLBuffer> vWeightsBuffer = [device newBufferWithBytes:vWeights.data()
                                                          length:vWeights.size() * sizeof(float)
                                                         options:MTLResourceStorageModeShared];

        // Create command buffer
        id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
        if (!commandBuffer) {
            NSLog(@"MetalGaussBlur: Failed to create command buffer");
            result = false;
            return;
        }

        // Calculate thread group size
        MTLSize threadGroupSize = MTLSizeMake(16, 16, 1);
        MTLSize gridSize = MTLSizeMake(
            (width + threadGroupSize.width - 1) / threadGroupSize.width * threadGroupSize.width,
            (height + threadGroupSize.height - 1) / threadGroupSize.height * threadGroupSize.height,
            1
        );

        // Horizontal blur pass (input -> temp)
        if (hSigma >= MIN_SIGMA) {
            id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
            [encoder setComputePipelineState:hBlurPipeline];
            [encoder setTexture:inputTexture atIndex:0];
            [encoder setTexture:tempTexture atIndex:1];
            [encoder setBuffer:hWeightsBuffer offset:0 atIndex:0];

            simd_int3 params = simd_make_int3(hRadius, width, height);
            [encoder setBytes:&params length:sizeof(params) atIndex:1];

            [encoder dispatchThreads:MTLSizeMake(width, height, 1) threadsPerThreadgroup:threadGroupSize];
            [encoder endEncoding];
        } else {
            // No horizontal blur - copy input to temp
            id<MTLBlitCommandEncoder> blit = [commandBuffer blitCommandEncoder];
            [blit copyFromTexture:inputTexture toTexture:tempTexture];
            [blit endEncoding];
        }

        // Vertical blur pass (temp -> output)
        if (vSigma >= MIN_SIGMA) {
            id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
            [encoder setComputePipelineState:vBlurPipeline];
            [encoder setTexture:tempTexture atIndex:0];
            [encoder setTexture:outputTexture atIndex:1];
            [encoder setBuffer:vWeightsBuffer offset:0 atIndex:0];

            simd_int3 params = simd_make_int3(vRadius, width, height);
            [encoder setBytes:&params length:sizeof(params) atIndex:1];

            [encoder dispatchThreads:MTLSizeMake(width, height, 1) threadsPerThreadgroup:threadGroupSize];
            [encoder endEncoding];
        } else {
            // No vertical blur - copy temp to output
            id<MTLBlitCommandEncoder> blit = [commandBuffer blitCommandEncoder];
            [blit copyFromTexture:tempTexture toTexture:outputTexture];
            [blit endEncoding];
        }

        // Commit and wait for completion
        [commandBuffer commit];
        [commandBuffer waitUntilCompleted];

        if (commandBuffer.status == MTLCommandBufferStatusError) {
            NSLog(@"MetalGaussBlur: Command buffer error: %@", commandBuffer.error);
            result = false;
            return;
        }

        // Read back results (convert float -> uint8)
        {
            std::vector<float> floatData(width * height);
            MTLRegion region = MTLRegionMake2D(0, 0, width, height);
            [outputTexture getBytes:floatData.data() bytesPerRow:width * sizeof(float) fromRegion:region mipmapLevel:0];

            for (int y = 0; y < height; y++) {
                uint8_t* row = data + y * stride;
                for (int x = 0; x < width; x++) {
                    float v = floatData[y * width + x] * 255.0f;
                    // Clamp and round
                    v = std::max(0.0f, std::min(255.0f, v));
                    row[x] = static_cast<uint8_t>(v + 0.5f);
                }
            }
        }

        result = true;
    }
    });  // end dispatch_sync
    return result;
}
