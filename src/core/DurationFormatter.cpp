// Copyright (C) 2026  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "DurationFormatter.h"

#include <QString>

namespace core {

QString formatDuration(const qint64 elapsedMilliseconds) {
  const qint64 nonNegativeMilliseconds = qMax<qint64>(0, elapsedMilliseconds);
  if (nonNegativeMilliseconds < 60000) {
    return QStringLiteral("%1 s").arg(nonNegativeMilliseconds / 1000.0, 0, 'f', 1);
  }

  const qint64 totalSeconds = nonNegativeMilliseconds / 1000;
  if (nonNegativeMilliseconds < 3600000) {
    return QStringLiteral("%1 m %2 s (%3 s)")
        .arg(totalSeconds / 60)
        .arg(totalSeconds % 60)
        .arg(totalSeconds);
  }

  return QStringLiteral("%1 h %2 m (%3 s)")
      .arg(totalSeconds / 3600)
      .arg((totalSeconds % 3600) / 60)
      .arg(totalSeconds);
}

}  // namespace core
