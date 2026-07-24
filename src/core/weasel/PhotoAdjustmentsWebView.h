// Copyright (C) 2026  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_WEASEL_PHOTOADJUSTMENTSWEBVIEW_H_
#define SCANTAILOR_WEASEL_PHOTOADJUSTMENTSWEBVIEW_H_

#include <QObject>
#include <QString>
#include <QVariantMap>

#include "PhotoAdjustments.h"
#include "WebOptionsPanelBase.h"

namespace weasel {

/**
 * Bridge object exposed to JavaScript as 'bridge'. JS calls its public slots
 * and connects to its valuesChanged signal.
 */
class PhotoAdjustmentsBridge : public QObject {
  Q_OBJECT
 public:
  explicit PhotoAdjustmentsBridge(QObject* parent = nullptr);

  void setAdjustments(const PhotoAdjustments& adj);
  PhotoAdjustments adjustments() const { return m_adjustments; }

 signals:
  void valuesChanged(const QVariantMap& values);
  void adjustmentsEditedByUser();
  void actionTriggered(const QString& action);

 public slots:
  // Called from JavaScript
  void setValue(const QString& id, double value);
  void requestValues();
  void action(const QString& name);

 private:
  QVariantMap toVariantMap() const;

  PhotoAdjustments m_adjustments;
};

/**
 * Web-based Photo Adjustments panel. Replaces the Qt slider grid.
 */
class PhotoAdjustmentsPanel : public WebOptionsPanelBase {
  Q_OBJECT
 public:
  explicit PhotoAdjustmentsPanel(QWidget* parent = nullptr);

  void setAdjustments(const PhotoAdjustments& adj);
  PhotoAdjustments adjustments() const;

 signals:
  void adjustmentsChanged();
  void autoRequested();
  void resetRequested();

 private slots:
  void onAction(const QString& action);

 private:
  PhotoAdjustmentsBridge* m_bridge;
};

// Legacy name retained for source-compat during migration.
using PhotoAdjustmentsWebView = PhotoAdjustmentsPanel;

}  // namespace weasel

#endif
