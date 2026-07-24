// Copyright (C) 2026  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_CORE_DURATIONFORMATTER_H_
#define SCANTAILOR_CORE_DURATIONFORMATTER_H_

#include <QString>
#include <QtGlobal>

namespace core {

QString formatDuration(qint64 elapsedMilliseconds);

}  // namespace core

#endif  // SCANTAILOR_CORE_DURATIONFORMATTER_H_
