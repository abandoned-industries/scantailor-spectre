// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_OUTPUT_COLORPARAMS_H_
#define SCANTAILOR_OUTPUT_COLORPARAMS_H_

#include "BlackWhiteOptions.h"
#include "ColorCommonOptions.h"
#include "SplittingOptions.h"

class QDomDocument;
class QDomElement;

namespace output {
// Note: AUTO_DETECT is a special value used only in the UI to trigger re-detection.
// It is never stored - the actual detected mode (B&W, Color, Mixed) is stored instead.
// COLOR outputs RGB, GRAYSCALE outputs 8-bit grayscale regardless of input.
// COLOR_GRAYSCALE is kept for backward compatibility (behaves like COLOR - auto-detects).
enum ColorMode { BLACK_AND_WHITE, COLOR_GRAYSCALE, MIXED, COLOR, GRAYSCALE, AUTO_DETECT = -1 };

class ColorParams {
 public:
  ColorParams();

  explicit ColorParams(const QDomElement& el);

  QDomElement toXml(QDomDocument& doc, const QString& name) const;

  ColorMode colorMode() const;

  void setColorMode(ColorMode mode);

  /**
   * Returns true if the color mode was explicitly set by the user,
   * false if it's the default value or was auto-detected.
   * When false, the system may re-run auto-detection.
   */
  bool isColorModeUserSet() const;

  /**
   * Mark the color mode as user-set. Call this when the user
   * explicitly selects a mode (not when auto-detecting).
   */
  void setColorModeUserSet(bool userSet);

  const ColorCommonOptions& colorCommonOptions() const;

  void setColorCommonOptions(const ColorCommonOptions& opt);

  const BlackWhiteOptions& blackWhiteOptions() const;

  void setBlackWhiteOptions(const BlackWhiteOptions& opt);

 private:
  static ColorMode parseColorMode(const QString& str);

  static QString formatColorMode(ColorMode mode);

  ColorMode m_colorMode;
  bool m_colorModeUserSet;  // true if user explicitly chose this mode
  ColorCommonOptions m_colorCommonOptions;
  BlackWhiteOptions m_bwOptions;
};


inline ColorMode ColorParams::colorMode() const {
  return m_colorMode;
}

inline void ColorParams::setColorMode(ColorMode mode) {
  m_colorMode = mode;
}

inline const ColorCommonOptions& ColorParams::colorCommonOptions() const {
  return m_colorCommonOptions;
}

inline void ColorParams::setColorCommonOptions(const ColorCommonOptions& opt) {
  m_colorCommonOptions = opt;
}

inline const BlackWhiteOptions& ColorParams::blackWhiteOptions() const {
  return m_bwOptions;
}

inline void ColorParams::setBlackWhiteOptions(const BlackWhiteOptions& opt) {
  m_bwOptions = opt;
}
}  // namespace output
#endif  // ifndef SCANTAILOR_OUTPUT_COLORPARAMS_H_
