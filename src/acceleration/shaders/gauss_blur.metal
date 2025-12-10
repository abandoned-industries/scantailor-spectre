// Copyright (C) 2024 ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include <metal_stdlib>
using namespace metal;

// Maximum kernel radius supported (supports sigma up to ~25)
constant int MAX_KERNEL_RADIUS = 128;

/**
 * Horizontal Gaussian blur pass.
 *
 * Reads from input texture, writes blurred result to output texture.
 * Uses separable convolution - only blurs in horizontal direction.
 *
 * @param inTexture  Input grayscale texture (r8unorm or r32float)
 * @param outTexture Output texture for blurred result
 * @param weights    Gaussian kernel weights (center weight at index 0)
 * @param params     x = kernel radius, y = texture width, z = texture height
 * @param gid        Thread position in grid (one thread per output pixel)
 */
kernel void gaussBlurHorizontal(
    texture2d<float, access::read> inTexture [[texture(0)]],
    texture2d<float, access::write> outTexture [[texture(1)]],
    constant float* weights [[buffer(0)]],
    constant int3& params [[buffer(1)]],
    uint2 gid [[thread_position_in_grid]])
{
    int radius = params.x;
    int width = params.y;
    int height = params.z;

    // Bounds check
    if (gid.x >= uint(width) || gid.y >= uint(height)) {
        return;
    }

    float sum = 0.0;
    int x = int(gid.x);
    int y = int(gid.y);

    // Center pixel
    sum = inTexture.read(gid).r * weights[0];

    // Symmetric kernel - process both sides together
    for (int i = 1; i <= radius; i++) {
        float weight = weights[i];

        // Left neighbor (clamped to edge)
        int xl = max(0, x - i);
        float left = inTexture.read(uint2(xl, y)).r;

        // Right neighbor (clamped to edge)
        int xr = min(width - 1, x + i);
        float right = inTexture.read(uint2(xr, y)).r;

        sum += (left + right) * weight;
    }

    outTexture.write(float4(sum, 0.0, 0.0, 1.0), gid);
}

/**
 * Vertical Gaussian blur pass.
 *
 * Reads from horizontally-blurred texture, writes final result.
 * Completes the separable convolution by blurring vertically.
 *
 * @param inTexture  Input texture (result of horizontal pass)
 * @param outTexture Output texture for final blurred result
 * @param weights    Gaussian kernel weights (center weight at index 0)
 * @param params     x = kernel radius, y = texture width, z = texture height
 * @param gid        Thread position in grid (one thread per output pixel)
 */
kernel void gaussBlurVertical(
    texture2d<float, access::read> inTexture [[texture(0)]],
    texture2d<float, access::write> outTexture [[texture(1)]],
    constant float* weights [[buffer(0)]],
    constant int3& params [[buffer(1)]],
    uint2 gid [[thread_position_in_grid]])
{
    int radius = params.x;
    int width = params.y;
    int height = params.z;

    // Bounds check
    if (gid.x >= uint(width) || gid.y >= uint(height)) {
        return;
    }

    float sum = 0.0;
    int x = int(gid.x);
    int y = int(gid.y);

    // Center pixel
    sum = inTexture.read(gid).r * weights[0];

    // Symmetric kernel - process both sides together
    for (int i = 1; i <= radius; i++) {
        float weight = weights[i];

        // Top neighbor (clamped to edge)
        int yt = max(0, y - i);
        float top = inTexture.read(uint2(x, yt)).r;

        // Bottom neighbor (clamped to edge)
        int yb = min(height - 1, y + i);
        float bottom = inTexture.read(uint2(x, yb)).r;

        sum += (top + bottom) * weight;
    }

    outTexture.write(float4(sum, 0.0, 0.0, 1.0), gid);
}

/**
 * Combined single-pass Gaussian blur for small kernels.
 *
 * More efficient for small sigma values where the kernel fits in cache.
 * Performs full 2D convolution in one pass.
 *
 * @param inTexture  Input grayscale texture
 * @param outTexture Output texture for blurred result
 * @param hWeights   Horizontal Gaussian kernel weights
 * @param vWeights   Vertical Gaussian kernel weights
 * @param params     x = h_radius, y = v_radius, z = width, w = height
 * @param gid        Thread position in grid
 */
kernel void gaussBlurSinglePass(
    texture2d<float, access::read> inTexture [[texture(0)]],
    texture2d<float, access::write> outTexture [[texture(1)]],
    constant float* hWeights [[buffer(0)]],
    constant float* vWeights [[buffer(1)]],
    constant int4& params [[buffer(2)]],
    uint2 gid [[thread_position_in_grid]])
{
    int hRadius = params.x;
    int vRadius = params.y;
    int width = params.z;
    int height = params.w;

    // Bounds check
    if (gid.x >= uint(width) || gid.y >= uint(height)) {
        return;
    }

    float sum = 0.0;
    int cx = int(gid.x);
    int cy = int(gid.y);

    // Full 2D convolution
    for (int dy = -vRadius; dy <= vRadius; dy++) {
        int y = clamp(cy + dy, 0, height - 1);
        float vWeight = vWeights[abs(dy)];

        for (int dx = -hRadius; dx <= hRadius; dx++) {
            int x = clamp(cx + dx, 0, width - 1);
            float hWeight = hWeights[abs(dx)];

            float pixel = inTexture.read(uint2(x, y)).r;
            sum += pixel * hWeight * vWeight;
        }
    }

    outTexture.write(float4(sum, 0.0, 0.0, 1.0), gid);
}
