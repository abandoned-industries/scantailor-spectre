// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "OcrResult.h"

namespace ocr {

QDomElement OcrWord::toXml(QDomDocument& doc) const {
  QDomElement el = doc.createElement("word");
  el.setAttribute("text", text);
  el.setAttribute("x", QString::number(boundingBox.x(), 'f', 2));
  el.setAttribute("y", QString::number(boundingBox.y(), 'f', 2));
  el.setAttribute("w", QString::number(boundingBox.width(), 'f', 2));
  el.setAttribute("h", QString::number(boundingBox.height(), 'f', 2));
  el.setAttribute("confidence", QString::number(confidence, 'f', 3));
  return el;
}

OcrWord OcrWord::fromXml(const QDomElement& el) {
  OcrWord word;
  word.text = el.attribute("text");
  const qreal x = el.attribute("x", "0").toDouble();
  const qreal y = el.attribute("y", "0").toDouble();
  const qreal w = el.attribute("w", "0").toDouble();
  const qreal h = el.attribute("h", "0").toDouble();
  word.boundingBox = QRectF(x, y, w, h);
  word.confidence = el.attribute("confidence", "0").toFloat();
  return word;
}

OcrResult::OcrResult(const QDomElement& el) {
  m_imageWidth = el.attribute("imageWidth", "0").toInt();
  m_imageHeight = el.attribute("imageHeight", "0").toInt();
  m_language = el.attribute("language");

  QDomElement wordEl = el.firstChildElement("word");
  while (!wordEl.isNull()) {
    m_words.append(OcrWord::fromXml(wordEl));
    wordEl = wordEl.nextSiblingElement("word");
  }
}

QDomElement OcrResult::toXml(QDomDocument& doc, const QString& name) const {
  QDomElement el = doc.createElement(name);
  el.setAttribute("imageWidth", m_imageWidth);
  el.setAttribute("imageHeight", m_imageHeight);
  if (!m_language.isEmpty()) {
    el.setAttribute("language", m_language);
  }

  for (const OcrWord& word : m_words) {
    el.appendChild(word.toXml(doc));
  }

  return el;
}

void OcrResult::addWord(const OcrWord& word) {
  m_words.append(word);
}

QString OcrResult::fullText() const {
  QString result;
  for (const OcrWord& word : m_words) {
    if (!result.isEmpty()) {
      result += ' ';
    }
    result += word.text;
  }
  return result;
}

void OcrResult::setImageDimensions(int width, int height) {
  m_imageWidth = width;
  m_imageHeight = height;
}

}  // namespace ocr
