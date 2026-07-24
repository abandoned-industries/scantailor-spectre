// Copyright (C) 2026  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "PhotoAdjustmentsWebView.h"

namespace weasel {

/* ============================ PhotoAdjustmentsBridge ============================ */

PhotoAdjustmentsBridge::PhotoAdjustmentsBridge(QObject* parent) : QObject(parent) {}

void PhotoAdjustmentsBridge::setAdjustments(const PhotoAdjustments& adj) {
  m_adjustments = adj;
  emit valuesChanged(toVariantMap());
}

void PhotoAdjustmentsBridge::setValue(const QString& id, double value) {
  if (id == QLatin1String("temp")) m_adjustments.setTemp(value);
  else if (id == QLatin1String("tint")) m_adjustments.setTint(value);
  else if (id == QLatin1String("exposure")) m_adjustments.setExposure(value);
  else if (id == QLatin1String("contrast")) m_adjustments.setContrast(value);
  else if (id == QLatin1String("highlights")) m_adjustments.setHighlights(value);
  else if (id == QLatin1String("shadows")) m_adjustments.setShadows(value);
  else if (id == QLatin1String("whites")) m_adjustments.setWhites(value);
  else if (id == QLatin1String("blacks")) m_adjustments.setBlacks(value);
  else return;

  emit adjustmentsEditedByUser();
}

void PhotoAdjustmentsBridge::requestValues() {
  emit valuesChanged(toVariantMap());
}

void PhotoAdjustmentsBridge::action(const QString& name) {
  emit actionTriggered(name);
}

QVariantMap PhotoAdjustmentsBridge::toVariantMap() const {
  QVariantMap m;
  m[QStringLiteral("temp")] = m_adjustments.temp();
  m[QStringLiteral("tint")] = m_adjustments.tint();
  m[QStringLiteral("exposure")] = m_adjustments.exposure();
  m[QStringLiteral("contrast")] = m_adjustments.contrast();
  m[QStringLiteral("highlights")] = m_adjustments.highlights();
  m[QStringLiteral("shadows")] = m_adjustments.shadows();
  m[QStringLiteral("whites")] = m_adjustments.whites();
  m[QStringLiteral("blacks")] = m_adjustments.blacks();
  return m;
}

/* ============================ PhotoAdjustmentsPanel ============================ */

PhotoAdjustmentsPanel::PhotoAdjustmentsPanel(QWidget* parent)
    : WebOptionsPanelBase(QStringLiteral("photo_adjustments.html"), parent),
      m_bridge(new PhotoAdjustmentsBridge(this)) {
  registerBridge(m_bridge);

  setMinimumHeight(360);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);

  connect(m_bridge, &PhotoAdjustmentsBridge::adjustmentsEditedByUser,
          this, &PhotoAdjustmentsPanel::adjustmentsChanged);
  connect(m_bridge, &PhotoAdjustmentsBridge::actionTriggered,
          this, &PhotoAdjustmentsPanel::onAction);
}

void PhotoAdjustmentsPanel::setAdjustments(const PhotoAdjustments& adj) {
  m_bridge->setAdjustments(adj);
}

PhotoAdjustments PhotoAdjustmentsPanel::adjustments() const {
  return m_bridge->adjustments();
}

void PhotoAdjustmentsPanel::onAction(const QString& action) {
  if (action == QLatin1String("auto")) {
    emit autoRequested();
  } else if (action == QLatin1String("reset")) {
    emit resetRequested();
  }
}

}  // namespace weasel
