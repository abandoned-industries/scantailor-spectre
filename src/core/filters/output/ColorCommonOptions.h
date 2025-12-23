// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_OUTPUT_COLORCOMMONOPTIONS_H_
#define SCANTAILOR_OUTPUT_COLORCOMMONOPTIONS_H_

#include <AutoManualMode.h>

#include <algorithm>

class QString;
class QDomDocument;
class QDomElement;

namespace output {
enum FillingColor { FILL_BACKGROUND, FILL_WHITE, FILL_BLACK };

class ColorCommonOptions {
 public:
  class PosterizationOptions {
   public:
    PosterizationOptions();

    explicit PosterizationOptions(const QDomElement& el);

    QDomElement toXml(QDomDocument& doc, const QString& name) const;

    bool operator==(const PosterizationOptions& other) const;

    bool operator!=(const PosterizationOptions& other) const;

    bool isEnabled() const;

    void setEnabled(bool enabled);

    int getLevel() const;

    void setLevel(int level);

    bool isNormalizationEnabled() const;

    void setNormalizationEnabled(bool normalizationEnabled);

    bool isForceBlackAndWhite() const;

    void setForceBlackAndWhite(bool forceBlackAndWhite);

   private:
    bool m_isEnabled;
    int m_level;
    bool m_isNormalizationEnabled;
    bool m_forceBlackAndWhite;
  };

  ColorCommonOptions();

  explicit ColorCommonOptions(const QDomElement& el);

  QDomElement toXml(QDomDocument& doc, const QString& name) const;

  bool fillOffcut() const;

  void setFillOffcut(bool fillOffcut);

  bool fillMargins() const;

  void setFillMargins(bool val);

  bool normalizeIllumination() const;

  void setNormalizeIllumination(bool val);

  double wienerCoef() const;
  void setWienerCoef(double val);
  int wienerWindowSize() const;
  void setWienerWindowSize(int val);

  // Paper detection thresholds for illumination equalization
  int paperBrightnessThreshold() const;
  void setPaperBrightnessThreshold(int val);
  int paperSaturationThreshold() const;
  void setPaperSaturationThreshold(int val);
  double paperCoverageThreshold() const;
  void setPaperCoverageThreshold(double val);
  bool useAdaptiveDetection() const;
  void setUseAdaptiveDetection(bool val);

  FillingColor getFillingColor() const;

  void setFillingColor(FillingColor fillingColor);

  bool operator==(const ColorCommonOptions& other) const;

  bool operator!=(const ColorCommonOptions& other) const;

  const PosterizationOptions& getPosterizationOptions() const;

  void setPosterizationOptions(const PosterizationOptions& posterizationOptions);

 private:
  static FillingColor parseFillingColor(const QString& str);

  static QString formatFillingColor(FillingColor type);


  bool m_fillOffcut;
  bool m_fillMargins;
  bool m_normalizeIllumination;
  double m_wienerCoef;
  int m_wienerWindowSize;
  FillingColor m_fillingColor;
  PosterizationOptions m_posterizationOptions;

  // Paper detection thresholds for illumination equalization
  int m_paperBrightnessThreshold;   // Default: 120 (pixels brighter than this may be paper)
  int m_paperSaturationThreshold;   // Default: 60 (pixels less saturated than this may be paper)
  double m_paperCoverageThreshold;  // Default: 0.01 (min fraction of image that must be paper-like)
  bool m_useAdaptiveDetection;      // Default: true (sample margins to learn paper color)
};


inline FillingColor ColorCommonOptions::getFillingColor() const {
  return m_fillingColor;
}

inline void ColorCommonOptions::setFillingColor(FillingColor fillingColor) {
  ColorCommonOptions::m_fillingColor = fillingColor;
}

inline void ColorCommonOptions::setFillMargins(bool val) {
  m_fillMargins = val;
}

inline bool ColorCommonOptions::fillMargins() const {
  return m_fillMargins;
}

inline bool ColorCommonOptions::normalizeIllumination() const {
  return m_normalizeIllumination;
}

inline void ColorCommonOptions::setNormalizeIllumination(bool val) {
  m_normalizeIllumination = val;
}

inline double ColorCommonOptions::wienerCoef() const {
  return m_wienerCoef;
}
inline void ColorCommonOptions::setWienerCoef(double val) {
  m_wienerCoef = val;
}
inline int ColorCommonOptions::wienerWindowSize() const {
  return m_wienerWindowSize;
}
inline void ColorCommonOptions::setWienerWindowSize(int val) {
  m_wienerWindowSize = val;
}

inline const ColorCommonOptions::PosterizationOptions& ColorCommonOptions::getPosterizationOptions() const {
  return m_posterizationOptions;
}

inline void ColorCommonOptions::setPosterizationOptions(
    const ColorCommonOptions::PosterizationOptions& posterizationOptions) {
  ColorCommonOptions::m_posterizationOptions = posterizationOptions;
}

inline bool ColorCommonOptions::fillOffcut() const {
  return m_fillOffcut;
}

inline void ColorCommonOptions::setFillOffcut(bool fillOffcut) {
  m_fillOffcut = fillOffcut;
}

inline bool ColorCommonOptions::PosterizationOptions::isEnabled() const {
  return m_isEnabled;
}

inline void ColorCommonOptions::PosterizationOptions::setEnabled(bool enabled) {
  PosterizationOptions::m_isEnabled = enabled;
}

inline int ColorCommonOptions::PosterizationOptions::getLevel() const {
  return m_level;
}

inline void ColorCommonOptions::PosterizationOptions::setLevel(int level) {
  PosterizationOptions::m_level = level;
}

inline bool ColorCommonOptions::PosterizationOptions::isNormalizationEnabled() const {
  return m_isNormalizationEnabled;
}

inline void ColorCommonOptions::PosterizationOptions::setNormalizationEnabled(bool normalizationEnabled) {
  PosterizationOptions::m_isNormalizationEnabled = normalizationEnabled;
}

inline bool ColorCommonOptions::PosterizationOptions::isForceBlackAndWhite() const {
  return m_forceBlackAndWhite;
}

inline void ColorCommonOptions::PosterizationOptions::setForceBlackAndWhite(bool forceBlackAndWhite) {
  PosterizationOptions::m_forceBlackAndWhite = forceBlackAndWhite;
}

inline int ColorCommonOptions::paperBrightnessThreshold() const {
  return m_paperBrightnessThreshold;
}

inline void ColorCommonOptions::setPaperBrightnessThreshold(int val) {
  m_paperBrightnessThreshold = std::clamp(val, 0, 255);
}

inline int ColorCommonOptions::paperSaturationThreshold() const {
  return m_paperSaturationThreshold;
}

inline void ColorCommonOptions::setPaperSaturationThreshold(int val) {
  m_paperSaturationThreshold = std::clamp(val, 0, 255);
}

inline double ColorCommonOptions::paperCoverageThreshold() const {
  return m_paperCoverageThreshold;
}

inline void ColorCommonOptions::setPaperCoverageThreshold(double val) {
  m_paperCoverageThreshold = std::clamp(val, 0.0, 1.0);
}

inline bool ColorCommonOptions::useAdaptiveDetection() const {
  return m_useAdaptiveDetection;
}

inline void ColorCommonOptions::setUseAdaptiveDetection(bool val) {
  m_useAdaptiveDetection = val;
}
}  // namespace output
#endif  // ifndef SCANTAILOR_OUTPUT_COLORCOMMONOPTIONS_H_
