// Copyright (C) 2026  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "GenericPanelBridge.h"

namespace weasel {

GenericPanelBridge::GenericPanelBridge(QObject* parent) : QObject(parent) {}

void GenericPanelBridge::setState(const QVariantMap& values) {
  // Merge new values into existing state
  for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
    m_state[it.key()] = it.value();
  }
  emit valuesChanged(m_state);
}

}  // namespace weasel
