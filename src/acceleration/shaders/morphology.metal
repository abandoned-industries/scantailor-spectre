// Metal compute kernels for morphological operations (erosion/dilation)
// Uses separable passes: horizontal then vertical

#include <metal_stdlib>
using namespace metal;

// Horizontal erosion pass (min filter)
kernel void erodeHorizontal(
    texture2d<float, access::read> inTexture [[texture(0)]],
    texture2d<float, access::write> outTexture [[texture(1)]],
    constant int& radius [[buffer(0)]],
    constant float& borderValue [[buffer(1)]],
    uint2 gid [[thread_position_in_grid]])
{
    const int width = inTexture.get_width();
    const int height = inTexture.get_height();

    if (gid.x >= (uint)width || gid.y >= (uint)height) return;

    float minVal = inTexture.read(gid).r;

    for (int dx = -radius; dx <= radius; dx++) {
        int x = (int)gid.x + dx;
        float val;
        if (x < 0 || x >= width) {
            val = borderValue;
        } else {
            val = inTexture.read(uint2(x, gid.y)).r;
        }
        minVal = min(minVal, val);
    }

    outTexture.write(float4(minVal, minVal, minVal, 1.0), gid);
}

// Vertical erosion pass (min filter)
kernel void erodeVertical(
    texture2d<float, access::read> inTexture [[texture(0)]],
    texture2d<float, access::write> outTexture [[texture(1)]],
    constant int& radius [[buffer(0)]],
    constant float& borderValue [[buffer(1)]],
    uint2 gid [[thread_position_in_grid]])
{
    const int width = inTexture.get_width();
    const int height = inTexture.get_height();

    if (gid.x >= (uint)width || gid.y >= (uint)height) return;

    float minVal = inTexture.read(gid).r;

    for (int dy = -radius; dy <= radius; dy++) {
        int y = (int)gid.y + dy;
        float val;
        if (y < 0 || y >= height) {
            val = borderValue;
        } else {
            val = inTexture.read(uint2(gid.x, y)).r;
        }
        minVal = min(minVal, val);
    }

    outTexture.write(float4(minVal, minVal, minVal, 1.0), gid);
}

// Horizontal dilation pass (max filter)
kernel void dilateHorizontal(
    texture2d<float, access::read> inTexture [[texture(0)]],
    texture2d<float, access::write> outTexture [[texture(1)]],
    constant int& radius [[buffer(0)]],
    constant float& borderValue [[buffer(1)]],
    uint2 gid [[thread_position_in_grid]])
{
    const int width = inTexture.get_width();
    const int height = inTexture.get_height();

    if (gid.x >= (uint)width || gid.y >= (uint)height) return;

    float maxVal = inTexture.read(gid).r;

    for (int dx = -radius; dx <= radius; dx++) {
        int x = (int)gid.x + dx;
        float val;
        if (x < 0 || x >= width) {
            val = borderValue;
        } else {
            val = inTexture.read(uint2(x, gid.y)).r;
        }
        maxVal = max(maxVal, val);
    }

    outTexture.write(float4(maxVal, maxVal, maxVal, 1.0), gid);
}

// Vertical dilation pass (max filter)
kernel void dilateVertical(
    texture2d<float, access::read> inTexture [[texture(0)]],
    texture2d<float, access::write> outTexture [[texture(1)]],
    constant int& radius [[buffer(0)]],
    constant float& borderValue [[buffer(1)]],
    uint2 gid [[thread_position_in_grid]])
{
    const int width = inTexture.get_width();
    const int height = inTexture.get_height();

    if (gid.x >= (uint)width || gid.y >= (uint)height) return;

    float maxVal = inTexture.read(gid).r;

    for (int dy = -radius; dy <= radius; dy++) {
        int y = (int)gid.y + dy;
        float val;
        if (y < 0 || y >= height) {
            val = borderValue;
        } else {
            val = inTexture.read(uint2(gid.x, y)).r;
        }
        maxVal = max(maxVal, val);
    }

    outTexture.write(float4(maxVal, maxVal, maxVal, 1.0), gid);
}
