// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "OutputProcessingParams.h"

#include <QDomDocument>

#include "BlackWhiteOptions.h"

namespace output {

OutputProcessingParams::OutputProcessingParams()
    : m_autoZonesFound(false), m_blackOnWhiteSetManually(false), m_brightness(0.0), m_contrast(0.0) {}

OutputProcessingParams::OutputProcessingParams(const QDomElement& el)
    : m_autoZonesFound(el.attribute("autoZonesFound") == "1"),
      m_blackOnWhiteSetManually(el.attribute("blackOnWhiteSetManually") == "1"),
      m_brightness(el.attribute("brightness").toDouble()),
      m_contrast(el.attribute("contrast").toDouble()) {}

QDomElement OutputProcessingParams::toXml(QDomDocument& doc, const QString& name) const {
  QDomElement el(doc.createElement(name));
  el.setAttribute("autoZonesFound", m_autoZonesFound ? "1" : "0");
  el.setAttribute("blackOnWhiteSetManually", m_blackOnWhiteSetManually ? "1" : "0");
  el.setAttribute("brightness", m_brightness);
  el.setAttribute("contrast", m_contrast);
  return el;
}

bool OutputProcessingParams::operator==(const OutputProcessingParams& other) const {
  return (m_autoZonesFound == other.m_autoZonesFound) && (m_blackOnWhiteSetManually == other.m_blackOnWhiteSetManually)
         && (m_brightness == other.m_brightness) && (m_contrast == other.m_contrast);
}

bool OutputProcessingParams::operator!=(const OutputProcessingParams& other) const {
  return !(*this == other);
}
}  // namespace output
