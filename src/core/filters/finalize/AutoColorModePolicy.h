// Copyright (C) 2026 ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_FINALIZE_AUTOCOLORMODEPOLICY_H_
#define SCANTAILOR_FINALIZE_AUTOCOLORMODEPOLICY_H_

namespace finalize {

enum class AutoColorModePolicy {
  BestGuess,
  NeverColor,
  ForceBlackAndWhite
};

enum class ColorMode;

ColorMode applyAutoColorModePolicy(ColorMode detectedMode, AutoColorModePolicy policy);

}  // namespace finalize

#endif  // SCANTAILOR_FINALIZE_AUTOCOLORMODEPOLICY_H_
