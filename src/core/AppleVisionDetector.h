// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_CORE_APPLEVISIONDETECTOR_H_
#define SCANTAILOR_CORE_APPLEVISIONDETECTOR_H_

#include <QImage>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QVector>

/**
 * Apple Vision Framework integration for intelligent image analysis.
 * Uses macOS Vision APIs for document detection, text recognition,
 * and content classification. Falls back to basic detection on non-Apple platforms.
 */
class AppleVisionDetector {
 public:
  /**
   * High-level content classification for scanned pages.
   */
  enum class ContentType {
    Document,     // Text-heavy document (book page, letter, etc.)
    Photo,        // Photograph or continuous-tone image
    Illustration, // Line art, drawings, diagrams
    Mixed,        // Document with embedded images
    Unknown       // Could not determine
  };

  /**
   * Detected text region with confidence score.
   */
  struct TextRegion {
    QRectF bounds;       // Bounding rectangle in image coordinates
    float confidence;    // Detection confidence (0.0 - 1.0)
    QString text;        // Recognized text (if available)
  };

  /**
   * Document boundary detection result.
   */
  struct DocumentBounds {
    bool detected;           // Whether a document was found
    QPolygonF corners;       // Four corners of detected document
    QRectF boundingRect;     // Axis-aligned bounding rectangle
    float confidence;        // Detection confidence (0.0 - 1.0)
  };

  /**
   * Detected image/photo region within a document.
   */
  struct ImageRegion {
    QRectF bounds;           // Bounding rectangle in image coordinates
    float confidence;        // Detection confidence (0.0 - 1.0)
    bool hasFace;            // True if faces were detected in this region
    float colorfulness;      // How colorful this region is (0.0 - 1.0)
  };

  /**
   * Complete analysis result for an image.
   */
  struct AnalysisResult {
    ContentType contentType;
    DocumentBounds documentBounds;
    QVector<TextRegion> textRegions;
    QVector<ImageRegion> imageRegions;  // Detected photos/images within the document
    float textCoverage;          // Percentage of image covered by text (0.0 - 1.0)
    float imageCoverage;         // Percentage of image covered by photos/images (0.0 - 1.0)
    float overallColorfulness;   // Overall colorfulness of the image (0.0 - 1.0)
    bool isHighContrast;         // True if image has high contrast (good for B&W)
    bool hasEmbeddedImages;      // True if document contains photos or illustrations
    bool hasTonalContent;        // True if meaningful gradients/shading detected (preserve grayscale)
  };

  /**
   * Check if Apple Vision is available on this platform.
   */
  static bool isAvailable();

  /**
   * Analyze an image using Vision framework.
   * @param image The image to analyze
   * @return Complete analysis result
   */
  static AnalysisResult analyze(const QImage& image);

  /**
   * Analyze an image from file path.
   * @param imagePath Path to the image file
   * @return Complete analysis result
   */
  static AnalysisResult analyzeFromFile(const QString& imagePath);

  /**
   * Detect document boundaries in an image.
   * @param image The image to analyze
   * @return Document boundary detection result
   */
  static DocumentBounds detectDocumentBounds(const QImage& image);

  /**
   * Detect text regions in an image.
   * @param image The image to analyze
   * @return Vector of detected text regions
   */
  static QVector<TextRegion> detectTextRegions(const QImage& image);

  /**
   * Classify the content type of an image.
   * @param image The image to analyze
   * @return Detected content type
   */
  static ContentType classifyContent(const QImage& image);

  /**
   * Detect faces in an image (used to identify photo regions).
   * @param image The image to analyze
   * @return Vector of face bounding rectangles
   */
  static QVector<QRectF> detectFaces(const QImage& image);

  /**
   * Detect image/photo regions within a document using saliency and colorfulness analysis.
   * @param image The image to analyze
   * @param textRegions Already-detected text regions to exclude
   * @return Vector of detected image regions
   */
  static QVector<ImageRegion> detectImageRegions(const QImage& image, const QVector<TextRegion>& textRegions);

  /**
   * Suggest optimal color mode based on Vision analysis.
   * @param result Analysis result from analyze()
   * @return Suggested mode: "bw" for black/white, "grayscale", or "color"
   */
  static QString suggestColorMode(const AnalysisResult& result);

  /**
   * Page split detection result.
   */
  struct PageSplitResult {
    bool shouldSplit;        // True if a two-page layout was detected
    double splitLineX;       // X coordinate of suggested split line (normalized 0.0-1.0)
    float confidence;        // Detection confidence (0.0 - 1.0)
    int leftTextRegions;     // Number of text regions on left side
    int rightTextRegions;    // Number of text regions on right side
  };

  /**
   * Detect if an image contains two pages (like a book spread) and suggest split location.
   * Uses text region clustering to find natural page boundaries.
   * @param image The image to analyze
   * @return Page split detection result
   */
  static PageSplitResult detectPageSplit(const QImage& image);

  /**
   * Detect page split from file path (with automatic downscaling for performance).
   * @param imagePath Path to the image file
   * @return Page split detection result
   */
  static PageSplitResult detectPageSplitFromFile(const QString& imagePath);
};

#endif  // SCANTAILOR_CORE_APPLEVISIONDETECTOR_H_
