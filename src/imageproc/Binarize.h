// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_IMAGEPROC_BINARIZE_H_
#define SCANTAILOR_IMAGEPROC_BINARIZE_H_

#include <QSize>

class QImage;

namespace imageproc {
class BinaryImage;

/**
 * \brief Image binarization using Otsu's global thresholding method.
 *
 * N. Otsu (1979). "A threshold selection method from gray-level histograms".
 * http://en.wikipedia.org/wiki/Otsu%27s_method
 */
BinaryImage binarizeOtsu(const QImage& src);

/**
 * \brief Image binarization using Mokji's global thresholding method.
 *
 * M. M. Mokji, S. A. R. Abu-Bakar: Adaptive Thresholding Based on
 * Co-occurrence Matrix Edge Information. Asia International Conference on
 * Modelling and Simulation 2007: 444-450
 * http://www.academypublisher.com/jcp/vol02/no08/jcp02084452.pdf
 *
 * \param src The source image.  May be in any format.
 * \param maxEdgeWidth The maximum gradient length to consider.
 * \param minEdgeMagnitude The minimum color difference in a gradient.
 * \return A black and white image.
 */
BinaryImage binarizeMokji(const QImage& src, unsigned maxEdgeWidth = 3, unsigned minEdgeMagnitude = 20);

/**
 * \brief Image binarization using Sauvola's local thresholding method.
 *
 * Sauvola, J. and M. Pietikainen. 2000. "Adaptive document image binarization".
 * http://www.mediateam.oulu.fi/publications/pdf/24.pdf
 */
BinaryImage binarizeSauvola(const QImage& src, QSize windowSize, double k = 0.34, double delta = 0.0);

/**
 * \brief Image binarization using Wolf's local thresholding method.
 *
 * C. Wolf, J.M. Jolion, F. Chassaing. "Text localization, enhancement and
 * binarization in multimedia documents."
 * http://liris.cnrs.fr/christian.wolf/papers/icpr2002v.pdf
 *
 * \param src The image to binarize.
 * \param windowSize The dimensions of a pixel neighborhood to consider.
 * \param lowerBound The minimum possible gray level that can be made white.
 * \param upperBound The maximum possible gray level that can be made black.
 */
BinaryImage binarizeWolf(const QImage& src,
                         QSize windowSize,
                         unsigned char lowerBound = 1,
                         unsigned char upperBound = 254,
                         double k = 0.3,
                         double delta = 0.0);

/**
 * \brief Image binarization using Bradley's adaptive thresholding method.
 *
 * Derek Bradley, Gerhard Roth. 2005. "Adaptive Thresholding Using the Integral Image".
 * http://www.scs.carleton.ca/~roth/iit-publications-iti/docs/gerh-50002.pdf
 */
BinaryImage binarizeBradley(const QImage& src, QSize windowSize, double k = 0.34, double delta = 0.0);

/**
 * \brief Image binarization using Grad local/global thresholding method.
 *
 * Grad (aka "Gradient snip"), zvezdochiot 2024. "Adaptive/global document image binarization".
 */
BinaryImage binarizeGrad(const QImage& src,
                         QSize windowSize,
                         unsigned char lowerBound = 1,
                         unsigned char upperBound = 254,
                         double k = 0.3,
                         double delta = 0.0);

/**
 * \brief Image binarization using EdgeDiv (EdgePlus & BlurDiv) local/global thresholding method.
 *
 * EdgeDiv, zvezdochiot 2023. "Adaptive/global document image binarization".
 */
BinaryImage binarizeEdgeDiv(const QImage& src,
                            QSize windowSize,
                            double kep = 0.0,
                            double kdb = 0.0,
                            double delta = 0.0);

/**
 * \brief Image binarization using Niblack's local thresholding method.
 *
 * W. Niblack (1986). "An Introduction to Digital Image Processing".
 * Prentice Hall.
 *
 * Niblack = mean - k * stdev
 * Good for documents with uneven illumination.
 */
BinaryImage binarizeNiblack(const QImage& src, QSize windowSize, double k = 0.2, double delta = 0.0);

/**
 * \brief Image binarization using N.I.C.K.'s local thresholding method.
 *
 * Khurram Khurshid, Imran Siddiqi, Claudie Faure, Nicole Vincent.
 * "Comparison of Niblack inspired Binarization methods for ancient documents", 2009.
 *
 * NICK = mean - k * sqrt(stdev^2 + c*mean^2)
 * Particularly good for ancient/degraded documents with low contrast.
 */
BinaryImage binarizeNick(const QImage& src, QSize windowSize, double k = 0.1, double delta = 0.0);

/**
 * \brief Image binarization using Singh's local thresholding method.
 *
 * Singh, O. I., Sinam, T., James, O., & Singh, T. R. (2012).
 * "Local contrast and mean based thresholding technique in image binarization".
 * International Journal of Computer Applications.
 *
 * Uses local contrast relative to mean for adaptive thresholding.
 * Good for documents with variable contrast.
 */
BinaryImage binarizeSingh(const QImage& src, QSize windowSize, double k = 0.3, double delta = 0.0);

/**
 * \brief Image binarization using WAN (White and Non-white) method.
 *
 * WAN uses (mean + max) / 2 as base and adjusts by standard deviation.
 * WAN = base * (1.0 - k * (1.0 - (stdev + delta) / 128.0))
 * Good for documents with varying background brightness.
 */
BinaryImage binarizeWAN(const QImage& src, QSize windowSize, double k = 0.3, double delta = 0.0);

/**
 * \brief Image binarization using Multi-Scale method.
 *
 * Multi-scale approach using hierarchical block-based thresholding.
 * Computes local min/max at multiple resolution levels for adaptive threshold.
 * Good for documents with varying contrast and illumination.
 */
BinaryImage binarizeMultiScale(const QImage& src, QSize windowSize, double k = 0.5, double delta = 0.0);

/**
 * \brief Image binarization using Robust method.
 *
 * Robust = 255 - (surround + 255) * sc / (surround + sc)
 * where sc = surround - pixel, surround = local mean
 *
 * Creates a pre-processed image that enhances contrast, then uses Otsu.
 * Good for documents with shadows and uneven illumination.
 */
BinaryImage binarizeRobust(const QImage& src, QSize windowSize, double k = 0.2, double delta = 0.0);

/**
 * \brief Image binarization using Gatos' method.
 *
 * Gatos, B., Pratikakis, I., Perantonis, S.J. (2006).
 * "Adaptive degraded document image binarization".
 * Pattern Recognition.
 *
 * Uses Wiener filtering for noise reduction, Niblack for initial segmentation,
 * background estimation by interpolating from background pixels, and
 * adaptive threshold map calculation.
 *
 * \param src The source image to binarize.
 * \param windowSize The window size for local operations.
 * \param noiseSigma Estimated noise standard deviation (default 3.0).
 * \param k Niblack coefficient for initial segmentation (default -0.2).
 * \param delta Threshold adjustment (default 0.0).
 * \param q Background parameter q (default 0.6).
 * \param p Background parameter p (default 0.2).
 */
BinaryImage binarizeGatos(const QImage& src,
                          QSize windowSize,
                          double noiseSigma = 3.0,
                          double k = -0.2,
                          double delta = 0.0,
                          double q = 0.6,
                          double p = 0.2);

/**
 * \brief Image binarization using Window method.
 *
 * Window uses local mean, deviation, and global statistics for adaptive thresholding.
 * threshold = mean * (1 - k * md / kd)
 * where md and kd are computed from deviation statistics.
 *
 * Good for documents with varying illumination and contrast.
 */
BinaryImage binarizeWindow(const QImage& src, QSize windowSize, double k = 0.3, double delta = 0.0);

/**
 * \brief Image binarization using Fox method.
 *
 * Fox uses gradient-based adaptive thresholding relative to local mean.
 * Computes threshold based on normalized gradient and minimum gray value.
 *
 * Good for documents with smooth background gradients.
 */
BinaryImage binarizeFox(const QImage& src, QSize windowSize, double k = 0.5, double delta = 0.0);

/**
 * \brief Image binarization using Engraving method.
 *
 * Engraving uses Gaussian blur and overlay-style blending for thresholding.
 * Particularly good for engraved or printed documents with fine patterns.
 *
 * \param coef Blending coefficient (0.0 = pure mean, 1.0 = full effect)
 */
BinaryImage binarizeEngraving(const QImage& src, QSize windowSize, double coef = 0.5, double delta = 0.0);

/**
 * \brief Image binarization using BiModal histogram analysis.
 *
 * BiModal finds optimal threshold using iterative clustering of histogram
 * into two groups (foreground/background). Uses k-means-like approach
 * to find the threshold that best separates the two peaks.
 *
 * Good for documents with clear bimodal histogram (distinct text/background).
 */
BinaryImage binarizeBiModal(const QImage& src, double delta = 0.0);

/**
 * \brief Image binarization using Mean distance method.
 *
 * Mean uses the dominant gray value and standard deviation of distances
 * to determine threshold. Classifies pixels based on their distance from
 * the dominant value compared to the computed threshold.
 *
 * Good for documents with uniform background color.
 */
BinaryImage binarizeMean(const QImage& src, double delta = 0.0);

/**
 * \brief Image binarization using Grain filtering method.
 *
 * Grain pre-processes the image using local contrast enhancement and
 * double-blur to reduce grain/noise, then applies BiModal thresholding.
 *
 * Good for grainy or noisy document scans.
 */
BinaryImage binarizeGrain(const QImage& src, QSize windowSize, double k = 0.5, double delta = 0.0);

BinaryImage peakThreshold(const QImage& image);
}  // namespace imageproc
#endif
