// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "ColorCommonOptions.h"

#include <QDomDocument>
#include <cmath>

namespace output {
ColorCommonOptions::ColorCommonOptions()
    : m_fillOffcut(true),
      m_fillMargins(true),
      m_normalizeIllumination(true),
      m_fillingColor(FILL_WHITE),
      m_wienerCoef(0.0),
      m_wienerWindowSize(5),
      m_paperBrightnessThreshold(120),
      m_paperSaturationThreshold(60),
      m_paperCoverageThreshold(0.01),
      m_useAdaptiveDetection(true) {}

ColorCommonOptions::ColorCommonOptions(const QDomElement& el)
    : m_fillOffcut(el.attribute("fillOffcut") == "1"),
      m_fillMargins(el.attribute("fillMargins") == "1"),
      m_normalizeIllumination(el.attribute("normalizeIlluminationColor") == "1"),
      m_fillingColor(parseFillingColor(el.attribute("fillingColor"))),
      m_posterizationOptions(el.namedItem("posterization-options").toElement()),
      m_wienerCoef(el.attribute("wienerCoef").toDouble()),
      m_wienerWindowSize(el.attribute("wienerWinSize").toInt()),
      m_paperBrightnessThreshold(el.attribute("paperBrightnessThreshold", "120").toInt()),
      m_paperSaturationThreshold(el.attribute("paperSaturationThreshold", "60").toInt()),
      m_paperCoverageThreshold(el.attribute("paperCoverageThreshold", "0.01").toDouble()),
      m_useAdaptiveDetection(el.attribute("useAdaptiveDetection", "1") == "1") {
  if (m_wienerCoef < 0.0 || m_wienerCoef > 1.0) {
    m_wienerCoef = 0.0;
  }
  if (m_wienerWindowSize < 3) {
    m_wienerWindowSize = 5;
  }
  // Validate paper detection thresholds
  if (m_paperBrightnessThreshold < 0 || m_paperBrightnessThreshold > 255) {
    m_paperBrightnessThreshold = 120;
  }
  if (m_paperSaturationThreshold < 0 || m_paperSaturationThreshold > 255) {
    m_paperSaturationThreshold = 60;
  }
  if (m_paperCoverageThreshold < 0.0 || m_paperCoverageThreshold > 1.0) {
    m_paperCoverageThreshold = 0.01;
  }
}

QDomElement ColorCommonOptions::toXml(QDomDocument& doc, const QString& name) const {
  QDomElement el(doc.createElement(name));
  el.setAttribute("fillMargins", m_fillMargins ? "1" : "0");
  el.setAttribute("fillOffcut", m_fillOffcut ? "1" : "0");
  el.setAttribute("normalizeIlluminationColor", m_normalizeIllumination ? "1" : "0");
  el.setAttribute("fillingColor", formatFillingColor(m_fillingColor));
  el.appendChild(m_posterizationOptions.toXml(doc, "posterization-options"));
  el.setAttribute("wienerCoef", m_wienerCoef);
  el.setAttribute("wienerWinSize", m_wienerWindowSize);
  el.setAttribute("paperBrightnessThreshold", m_paperBrightnessThreshold);
  el.setAttribute("paperSaturationThreshold", m_paperSaturationThreshold);
  el.setAttribute("paperCoverageThreshold", m_paperCoverageThreshold);
  el.setAttribute("useAdaptiveDetection", m_useAdaptiveDetection ? "1" : "0");
  return el;
}

bool ColorCommonOptions::operator==(const ColorCommonOptions& other) const {
  return (m_normalizeIllumination == other.m_normalizeIllumination) && (m_fillMargins == other.m_fillMargins)
         && (m_fillOffcut == other.m_fillOffcut) && (m_fillingColor == other.m_fillingColor)
         && (m_posterizationOptions == other.m_posterizationOptions) && (m_wienerCoef == other.m_wienerCoef)
         && (m_wienerWindowSize == other.m_wienerWindowSize)
         && (m_paperBrightnessThreshold == other.m_paperBrightnessThreshold)
         && (m_paperSaturationThreshold == other.m_paperSaturationThreshold)
         && (std::abs(m_paperCoverageThreshold - other.m_paperCoverageThreshold) < 0.0001)
         && (m_useAdaptiveDetection == other.m_useAdaptiveDetection);
}

bool ColorCommonOptions::operator!=(const ColorCommonOptions& other) const {
  return !(*this == other);
}

FillingColor ColorCommonOptions::parseFillingColor(const QString& str) {
  if (str == "white") {
    return FILL_WHITE;
  } else if (str == "black") {
    return FILL_BLACK;
  } else {
    return FILL_BACKGROUND;
  }
}

QString ColorCommonOptions::formatFillingColor(FillingColor type) {
  QString str = "";
  switch (type) {
    case FILL_WHITE:
      str = "white";
      break;
    case FILL_BLACK:
      str = "black";
      break;
    case FILL_BACKGROUND:
      str = "background";
      break;
  }
  return str;
}

/*=============================== ColorCommonOptions::PosterizationOptions ==================================*/

ColorCommonOptions::PosterizationOptions::PosterizationOptions()
    : m_isEnabled(false), m_level(4), m_isNormalizationEnabled(false), m_forceBlackAndWhite(true) {}

ColorCommonOptions::PosterizationOptions::PosterizationOptions(const QDomElement& el)
    : m_isEnabled(el.attribute("enabled") == "1"),
      m_level(el.attribute("level").toInt()),
      m_isNormalizationEnabled(el.attribute("normalizationEnabled") == "1"),
      m_forceBlackAndWhite(el.attribute("forceBlackAndWhite") == "1") {}

QDomElement ColorCommonOptions::PosterizationOptions::toXml(QDomDocument& doc, const QString& name) const {
  QDomElement el(doc.createElement(name));
  el.setAttribute("enabled", m_isEnabled ? "1" : "0");
  el.setAttribute("level", m_level);
  el.setAttribute("normalizationEnabled", m_isNormalizationEnabled ? "1" : "0");
  el.setAttribute("forceBlackAndWhite", m_forceBlackAndWhite ? "1" : "0");
  return el;
}

bool ColorCommonOptions::PosterizationOptions::operator==(const ColorCommonOptions::PosterizationOptions& other) const {
  return (m_isEnabled == other.m_isEnabled) && (m_level == other.m_level)
         && (m_isNormalizationEnabled == other.m_isNormalizationEnabled)
         && (m_forceBlackAndWhite == other.m_forceBlackAndWhite);
}

bool ColorCommonOptions::PosterizationOptions::operator!=(const ColorCommonOptions::PosterizationOptions& other) const {
  return !(*this == other);
}
}  // namespace output
