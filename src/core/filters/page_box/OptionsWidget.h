// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_PAGE_BOX_OPTIONSWIDGET_H_
#define SCANTAILOR_PAGE_BOX_OPTIONSWIDGET_H_

#include <AutoManualMode.h>
#include <core/ConnectionManager.h>

#include <QRectF>
#include <QSizeF>
#include <memory>

#include "FilterOptionsWidget.h"
#include "PageId.h"
#include "PageSelectionAccessor.h"
#include "ui_OptionsWidget.h"

class PageInfo;

namespace page_box {
class Settings;

class OptionsWidget : public FilterOptionsWidget, private Ui::OptionsWidget {
  Q_OBJECT
 public:
  OptionsWidget(std::shared_ptr<Settings> settings, const PageSelectionAccessor& pageSelectionAccessor);
  ~OptionsWidget() override;

  void preUpdateUI(const PageInfo& pageInfo);
  void postUpdateUI(const QRectF& pageRect, AutoManualMode pageDetectionMode, bool fineTuneCorners);

 public slots:
  void manualPageRectSet(const QRectF& pageRect);
  void updatePageRectSize(const QSizeF& size);

 signals:
  void reloadRequested();
  void invalidateThumbnail(const PageId& pageId);
  void invalidateAllThumbnails();
  void pageRectChangedLocally(const QRectF& pageRect);
  void pageRectStateChanged(bool enabled);

 private slots:
  void pageDetectToggled(AutoManualMode mode);
  void fineTuningChanged(bool checked);
  void dimensionsChangedLocally(double);
  void showApplyToDialog();
  void applySelection(const std::set<PageId>& pages);

 private:
  void updatePageModeIndication(AutoManualMode mode);
  void updatePageDetectOptionsDisplay();
  void commitCurrentParams();
  void setupUiConnections();
  void updateSelectionIndicator();

  std::shared_ptr<Settings> m_settings;
  PageSelectionAccessor m_pageSelectionAccessor;
  PageId m_pageId;
  QRectF m_pageRect;
  AutoManualMode m_pageDetectionMode;
  bool m_fineTuneCorners;

  ConnectionManager m_connectionManager;
};
}  // namespace page_box
#endif
