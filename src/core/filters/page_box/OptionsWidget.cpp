// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "OptionsWidget.h"

#include <functional>
#include <utility>

#include "PageInfo.h"
#include "Settings.h"

#define CONNECT(...) m_connectionManager.addConnection(connect(__VA_ARGS__))

namespace page_box {
OptionsWidget::OptionsWidget(std::shared_ptr<Settings> settings, const PageSelectionAccessor& pageSelectionAccessor)
    : m_settings(std::move(settings)),
      m_pageSelectionAccessor(pageSelectionAccessor),
      m_pageDetectionMode(MODE_DISABLED),
      m_fineTuneCorners(false),
      m_connectionManager(std::bind(&OptionsWidget::setupUiConnections, this)) {
  setupUi(this);
  setupUiConnections();
}

OptionsWidget::~OptionsWidget() = default;

void OptionsWidget::preUpdateUI(const PageInfo& pageInfo) {
  auto block = m_connectionManager.getScopedBlock();

  m_pageId = pageInfo.id();

  const std::unique_ptr<Params> params(m_settings->getPageParams(m_pageId));
  if (params) {
    m_pageRect = params->pageRect();
    m_pageDetectionMode = params->pageDetectionMode();
    m_fineTuneCorners = params->isFineTuningEnabled();
  } else {
    m_pageRect = QRectF();
    m_pageDetectionMode = MODE_DISABLED;
    m_fineTuneCorners = false;
  }

  updatePageModeIndication(m_pageDetectionMode);
  updatePageDetectOptionsDisplay();
  updateSelectionIndicator();
}

void OptionsWidget::postUpdateUI(const QRectF& pageRect, AutoManualMode pageDetectionMode, bool fineTuneCorners) {
  auto block = m_connectionManager.getScopedBlock();

  m_pageRect = pageRect;
  m_pageDetectionMode = pageDetectionMode;
  m_fineTuneCorners = fineTuneCorners;

  updatePageModeIndication(pageDetectionMode);
  updatePageDetectOptionsDisplay();

  if (pageDetectionMode == MODE_MANUAL && pageRect.isValid()) {
    widthSpinBox->setValue(pageRect.width());
    heightSpinBox->setValue(pageRect.height());
  }
}

void OptionsWidget::manualPageRectSet(const QRectF& pageRect) {
  m_pageRect = pageRect;
  m_pageDetectionMode = MODE_MANUAL;

  updatePageModeIndication(MODE_MANUAL);
  updatePageDetectOptionsDisplay();
  commitCurrentParams();

  emit invalidateThumbnail(m_pageId);
}

void OptionsWidget::updatePageRectSize(const QSizeF& size) {
  auto block = m_connectionManager.getScopedBlock();
  widthSpinBox->setValue(size.width());
  heightSpinBox->setValue(size.height());
}

void OptionsWidget::pageDetectToggled(AutoManualMode mode) {
  m_pageDetectionMode = mode;
  updatePageDetectOptionsDisplay();
  commitCurrentParams();

  emit pageRectStateChanged(mode != MODE_DISABLED);
  emit reloadRequested();
}

void OptionsWidget::fineTuningChanged(bool checked) {
  m_fineTuneCorners = checked;
  commitCurrentParams();
  emit reloadRequested();
}

void OptionsWidget::dimensionsChangedLocally(double) {
  const QRectF newRect(m_pageRect.topLeft(), QSizeF(widthSpinBox->value(), heightSpinBox->value()));
  emit pageRectChangedLocally(newRect);
}

void OptionsWidget::showApplyToDialog() {
  // Simple apply: apply current page detection mode to all selected pages
  const std::set<PageId> selectedPages = m_pageSelectionAccessor.selectedPages();
  if (selectedPages.size() <= 1) {
    return;
  }

  const std::unique_ptr<Params> currentParams(m_settings->getPageParams(m_pageId));
  if (!currentParams) {
    return;
  }

  for (const PageId& pageId : selectedPages) {
    if (pageId == m_pageId) continue;
    std::unique_ptr<Params> targetParams(m_settings->getPageParams(pageId));
    if (targetParams) {
      targetParams->setPageDetectionMode(m_pageDetectionMode);
      targetParams->setFineTuneCornersEnabled(m_fineTuneCorners);
      m_settings->setPageParams(pageId, *targetParams);
    }
  }

  emit invalidateAllThumbnails();
  emit reloadRequested();
}

void OptionsWidget::updatePageModeIndication(AutoManualMode mode) {
  auto block = m_connectionManager.getScopedBlock();
  pageDetectDisableBtn->setChecked(mode == MODE_DISABLED);
  pageDetectAutoBtn->setChecked(mode == MODE_AUTO);
  pageDetectManualBtn->setChecked(mode == MODE_MANUAL);
}

void OptionsWidget::updatePageDetectOptionsDisplay() {
  const bool showOptions = (m_pageDetectionMode != MODE_DISABLED);
  pageDetectOptions->setVisible(showOptions);

  if (showOptions) {
    fineTuneBtn->setChecked(m_fineTuneCorners);
    fineTuneBtn->setVisible(m_pageDetectionMode == MODE_AUTO);
    dimensionsWidget->setVisible(m_pageDetectionMode == MODE_MANUAL);
  }
}

void OptionsWidget::commitCurrentParams() {
  std::unique_ptr<Params> params(m_settings->getPageParams(m_pageId));
  if (params) {
    params->setPageDetectionMode(m_pageDetectionMode);
    params->setFineTuneCornersEnabled(m_fineTuneCorners);
    if (m_pageRect.isValid()) {
      params->setPageRect(m_pageRect);
    }
    m_settings->setPageParams(m_pageId, *params);
  }
}

void OptionsWidget::setupUiConnections() {
  connect(pageDetectDisableBtn, &QPushButton::pressed, this, [this] { pageDetectToggled(MODE_DISABLED); });
  connect(pageDetectAutoBtn, &QPushButton::pressed, this, [this] { pageDetectToggled(MODE_AUTO); });
  connect(pageDetectManualBtn, &QPushButton::pressed, this, [this] { pageDetectToggled(MODE_MANUAL); });
  CONNECT(fineTuneBtn, SIGNAL(toggled(bool)), this, SLOT(fineTuningChanged(bool)));
  CONNECT(widthSpinBox, SIGNAL(valueChanged(double)), this, SLOT(dimensionsChangedLocally(double)));
  CONNECT(heightSpinBox, SIGNAL(valueChanged(double)), this, SLOT(dimensionsChangedLocally(double)));
  CONNECT(applyToBtn, SIGNAL(clicked()), this, SLOT(showApplyToDialog()));
}

void OptionsWidget::updateSelectionIndicator() {
  const std::set<PageId> selectedPages = m_pageSelectionAccessor.selectedPages();
  if (selectedPages.size() > 1) {
    selectionIndicatorLabel->setText(tr("%1 pages selected").arg(selectedPages.size()));
    selectionIndicatorLabel->setVisible(true);
  } else {
    selectionIndicatorLabel->setVisible(false);
  }
}
}  // namespace page_box
