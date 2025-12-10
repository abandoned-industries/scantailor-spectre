// Copyright (C) 2024 ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_ACCELERATION_METALMORPHOLOGY_H_
#define SCANTAILOR_ACCELERATION_METALMORPHOLOGY_H_

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Check if Metal morphology operations are available.
 * @return true if Metal is available and morphology kernels are loaded
 */
bool metalMorphologyAvailable(void);

/**
 * Perform grayscale erosion using Metal GPU acceleration.
 * Erosion spreads lighter (higher) pixel values.
 *
 * @param data Pointer to grayscale image data (modified in-place)
 * @param width Image width in pixels
 * @param height Image height in pixels
 * @param stride Row stride in bytes
 * @param brickWidth Width of the structuring element
 * @param brickHeight Height of the structuring element
 * @param borderValue Value assumed for pixels outside the image (0x00 = black, 0xFF = white)
 * @return true if GPU processing succeeded, false if CPU fallback is needed
 */
bool metalErodeGray(uint8_t* data, int width, int height, int stride,
                    int brickWidth, int brickHeight, uint8_t borderValue);

/**
 * Perform grayscale dilation using Metal GPU acceleration.
 * Dilation spreads darker (lower) pixel values.
 *
 * @param data Pointer to grayscale image data (modified in-place)
 * @param width Image width in pixels
 * @param height Image height in pixels
 * @param stride Row stride in bytes
 * @param brickWidth Width of the structuring element
 * @param brickHeight Height of the structuring element
 * @param borderValue Value assumed for pixels outside the image (0x00 = black, 0xFF = white)
 * @return true if GPU processing succeeded, false if CPU fallback is needed
 */
bool metalDilateGray(uint8_t* data, int width, int height, int stride,
                     int brickWidth, int brickHeight, uint8_t borderValue);

#ifdef __cplusplus
}
#endif

#endif  // SCANTAILOR_ACCELERATION_METALMORPHOLOGY_H_
