// Copyright (C) 2026  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_PAGE_SPLIT_SPLITMODEDIALOGMODE_H_
#define SCANTAILOR_PAGE_SPLIT_SPLITMODEDIALOGMODE_H_

#include "AutoManualMode.h"
#include "LayoutType.h"

namespace page_split {
inline bool splitModeDialogOpensManual(const LayoutType layoutType, const AutoManualMode splitLineMode) {
  return (layoutType != AUTO_LAYOUT_TYPE) || (splitLineMode == MODE_MANUAL);
}
}  // namespace page_split

#endif  // ifndef SCANTAILOR_PAGE_SPLIT_SPLITMODEDIALOGMODE_H_
