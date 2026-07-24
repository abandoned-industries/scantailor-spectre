// Copyright (C) 2026 ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "AutoColorDecisionReset.h"

#include "PageSequence.h"
#include "Settings.h"
#include "../output/Settings.h"

namespace finalize {

void clearAutomaticColorDecisions(Settings& finalizeSettings,
                                  output::Settings& outputSettings,
                                  const PageSequence& pages) {
  // Clear every stored Finalize record, including legacy PageIds that may no
  // longer be present in the current page sequence.
  finalizeSettings.clearDetectionCache();

  for (const PageInfo& pageInfo : pages) {
    if (outputSettings.isParamsNull(pageInfo.id())) {
      outputSettings.removeOutputParams(pageInfo.id());
      continue;
    }

    const output::ColorParams colorParams = outputSettings.getParams(pageInfo.id()).colorParams();
    if (!colorParams.isColorModeUserSet() || colorParams.isColorModePresetSet()) {
      // Output compares this fingerprint with the requested render parameters.
      // Removing it guarantees a render even if the new detector reaches the
      // same mode as the stale verdict.
      outputSettings.removeOutputParams(pageInfo.id());
    }
  }
}

}  // namespace finalize
