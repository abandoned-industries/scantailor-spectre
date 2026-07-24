// Copyright (C) 2026 ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_FINALIZE_AUTOCOLORDECISIONRESET_H_
#define SCANTAILOR_FINALIZE_AUTOCOLORDECISIONRESET_H_

class PageSequence;

namespace output {
class Settings;
}

namespace finalize {

class Settings;

/**
 * Clears cached automatic Finalize verdicts and invalidates rendered Output
 * fingerprints for pages whose color mode is managed automatically.
 *
 * Explicit manual Output color modes are authoritative and are left intact.
 */
void clearAutomaticColorDecisions(Settings& finalizeSettings,
                                  output::Settings& outputSettings,
                                  const PageSequence& pages);

}  // namespace finalize

#endif  // SCANTAILOR_FINALIZE_AUTOCOLORDECISIONRESET_H_
