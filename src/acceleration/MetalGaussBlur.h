// Copyright (C) 2024 ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_ACCELERATION_METALGAUSSBLUR_H_
#define SCANTAILOR_ACCELERATION_METALGAUSSBLUR_H_

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Apply Gaussian blur using Metal GPU acceleration.
 *
 * This function performs a separable Gaussian blur on grayscale image data
 * using the GPU via Metal compute shaders. It's designed to be a drop-in
 * replacement for CPU-based Gaussian blur operations.
 *
 * @param data      Pointer to 8-bit grayscale image data (modified in-place)
 * @param width     Image width in pixels
 * @param height    Image height in pixels
 * @param stride    Number of bytes per row (may be larger than width due to alignment)
 * @param hSigma    Horizontal blur sigma (standard deviation)
 * @param vSigma    Vertical blur sigma (standard deviation)
 * @return          true if GPU blur was performed, false if fallback to CPU is needed
 *
 * @note Returns false (fallback needed) in these cases:
 *       - Metal is not available
 *       - Image is too small (GPU overhead not worth it)
 *       - Sigma is too small (essentially no blur)
 *       - Any GPU error occurs
 */
bool metalGaussBlur(uint8_t* data, int width, int height, int stride,
                    float hSigma, float vSigma);

/**
 * @brief Check if Metal Gaussian blur acceleration is available.
 * @return true if Metal blur can be used, false otherwise.
 */
bool metalGaussBlurAvailable(void);

#ifdef __cplusplus
}
#endif

#endif // SCANTAILOR_ACCELERATION_METALGAUSSBLUR_H_
