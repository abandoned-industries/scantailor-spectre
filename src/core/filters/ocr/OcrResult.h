// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_OCR_OCRRESULT_H_
#define SCANTAILOR_OCR_OCRRESULT_H_

#include <QDomDocument>
#include <QDomElement>
#include <QRectF>
#include <QString>
#include <QVector>

namespace ocr {

/**
 * Single recognized word with bounding box.
 */
struct OcrWord {
  QString text;
  QRectF boundingBox;  // In image coordinates (pixels)
  float confidence;    // 0.0 - 1.0

  OcrWord() : confidence(0.0f) {}
  OcrWord(const QString& t, const QRectF& box, float conf) : text(t), boundingBox(box), confidence(conf) {}

  QDomElement toXml(QDomDocument& doc) const;
  static OcrWord fromXml(const QDomElement& el);
};

/**
 * OCR result for an entire page.
 */
class OcrResult {
 public:
  OcrResult() = default;
  explicit OcrResult(const QDomElement& el);

  QDomElement toXml(QDomDocument& doc, const QString& name) const;

  void addWord(const OcrWord& word);
  const QVector<OcrWord>& words() const { return m_words; }
  void clear() { m_words.clear(); }

  bool isEmpty() const { return m_words.isEmpty(); }
  QString fullText() const;  // Concatenated text for display

  // Image dimensions at time of OCR (for coordinate scaling)
  int imageWidth() const { return m_imageWidth; }
  int imageHeight() const { return m_imageHeight; }
  void setImageDimensions(int width, int height);

  // Detection language used
  QString language() const { return m_language; }
  void setLanguage(const QString& lang) { m_language = lang; }

 private:
  QVector<OcrWord> m_words;
  int m_imageWidth = 0;
  int m_imageHeight = 0;
  QString m_language;
};

}  // namespace ocr

#endif  // SCANTAILOR_OCR_OCRRESULT_H_
