// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "OptionsWidget.h"

#include <UnitsProvider.h>

#include <boost/bind/bind.hpp>
#include <utility>

#include "ApplyDialog.h"
#include "Settings.h"

namespace select_content {
OptionsWidget::OptionsWidget(std::shared_ptr<Settings> settings, const PageSelectionAccessor& pageSelectionAccessor)
    : m_settings(std::move(settings)),
      m_pageSelectionAccessor(pageSelectionAccessor),
      m_connectionManager(std::bind(&OptionsWidget::setupUiConnections, this)) {
  setupUi(this);

  setupUiConnections();
  updateDetectionSettingsUI();
}

OptionsWidget::~OptionsWidget() = default;

void OptionsWidget::preUpdateUI(const PageInfo& pageInfo) {
  auto block = m_connectionManager.getScopedBlock();

  m_pageId = pageInfo.id();
  m_dpi = pageInfo.metadata().dpi();

  updateSelectionIndicator();

  contentBoxGroup->setEnabled(false);

  onUnitsChanged(UnitsProvider::getInstance().getUnits());
}

void OptionsWidget::postUpdateUI(const UiData& uiData) {
  auto block = m_connectionManager.getScopedBlock();

  m_uiData = uiData;

  updateContentModeIndication(uiData.contentDetectionMode());

  contentBoxGroup->setEnabled(true);
}

void OptionsWidget::manualContentRectSet(const QRectF& contentRect) {
  m_uiData.setContentRect(contentRect);
  m_uiData.setContentDetectionMode(MODE_MANUAL);
  updateContentModeIndication(MODE_MANUAL);

  commitCurrentParams();

  emit invalidateThumbnail(m_pageId);
}

void OptionsWidget::contentDetectToggled(const AutoManualMode mode) {
  m_uiData.setContentDetectionMode(mode);
  commitCurrentParams();

  // Apply mode to all selected pages
  applyModeToSelectedPages();

  if (mode != MODE_MANUAL) {
    emit reloadRequested();
  }
}

void OptionsWidget::updateContentModeIndication(const AutoManualMode mode) {
  switch (mode) {
    case MODE_AUTO:
      contentDetectAutoBtn->setChecked(true);
      break;
    case MODE_MANUAL:
      contentDetectManualBtn->setChecked(true);
      break;
    case MODE_DISABLED:
      contentDetectDisableBtn->setChecked(true);
      break;
  }
}

void OptionsWidget::commitCurrentParams() {
  updateDependenciesIfNecessary();

  Params params(m_uiData.contentRect(), m_uiData.contentSizeMM(), m_uiData.pageRect(), m_uiData.dependencies(),
                m_uiData.contentDetectionMode(), m_uiData.pageDetectionMode(), m_uiData.isFineTuningCornersEnabled());
  m_settings->setPageParams(m_pageId, params);
}

void OptionsWidget::updateDependenciesIfNecessary() {
  // On switching to manual mode the page dependencies isn't updated
  // as Task::process isn't called, so we need to update it manually.
  if (!(m_uiData.contentDetectionMode() == MODE_MANUAL || m_uiData.pageDetectionMode() == MODE_MANUAL)) {
    return;
  }

  Dependencies deps = m_uiData.dependencies();
  if (m_uiData.contentDetectionMode() == MODE_MANUAL) {
    deps.setContentDetectionMode(MODE_MANUAL);
  }
  if (m_uiData.pageDetectionMode() == MODE_MANUAL) {
    deps.setPageDetectionMode(MODE_MANUAL);
  }
  m_uiData.setDependencies(deps);
}

void OptionsWidget::showApplyToDialog() {
  auto* dialog = new ApplyDialog(this, m_pageId, m_pageSelectionAccessor);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  connect(dialog, SIGNAL(applySelection(const std::set<PageId>&, bool, bool)), this,
          SLOT(applySelection(const std::set<PageId>&, bool, bool)));
  dialog->show();
}

void OptionsWidget::applySelection(const std::set<PageId>& pages, const bool applyContentBox, const bool applyPageBox) {
  if (pages.empty()) {
    return;
  }

  const Params params(m_uiData.contentRect(), m_uiData.contentSizeMM(), m_uiData.pageRect(), Dependencies(),
                      m_uiData.contentDetectionMode(), m_uiData.pageDetectionMode(),
                      m_uiData.isFineTuningCornersEnabled());

  for (const PageId& pageId : pages) {
    if (m_pageId == pageId) {
      continue;
    }

    Params newParams(params);
    std::unique_ptr<Params> oldParams = m_settings->getPageParams(pageId);
    if (oldParams) {
      if (newParams.pageDetectionMode() == MODE_MANUAL) {
        if (!applyPageBox) {
          newParams.setPageRect(oldParams->pageRect());
        } else {
          QRectF correctedPageRect = newParams.pageRect();
          const QRectF sourceImageRect = newParams.dependencies().rotatedPageOutline().boundingRect();
          const QRectF currentImageRect = oldParams->dependencies().rotatedPageOutline().boundingRect();
          if (sourceImageRect.isValid() && currentImageRect.isValid()) {
            correctedPageRect.translate((currentImageRect.width() - sourceImageRect.width()) / 2,
                                        (currentImageRect.height() - sourceImageRect.height()) / 2);
            newParams.setPageRect(correctedPageRect);
          }
        }
      }
      if (newParams.contentDetectionMode() == MODE_MANUAL) {
        if (!applyContentBox) {
          newParams.setContentRect(oldParams->contentRect());
        } else if (!newParams.contentRect().isEmpty()) {
          QRectF correctedContentRect = newParams.contentRect();
          const QRectF& sourcePageRect = m_uiData.pageRect();
          const QRectF& newPageRect = newParams.pageRect();
          correctedContentRect.translate(newPageRect.x() - sourcePageRect.x(), newPageRect.y() - sourcePageRect.y());
          newParams.setContentRect(correctedContentRect);
        }
      }
    }

    m_settings->setPageParams(pageId, newParams);
  }

  if (pages.size() > 1) {
    emit invalidateAllThumbnails();
  } else {
    for (const PageId& pageId : pages) {
      emit invalidateThumbnail(pageId);
    }
  }

  emit reloadRequested();
}  // OptionsWidget::applySelection


void OptionsWidget::onUnitsChanged(Units units) {
  auto block = m_connectionManager.getScopedBlock();

  int decimals;
  double step;
  switch (units) {
    case PIXELS:
    case MILLIMETRES:
      decimals = 1;
      step = 1.0;
      break;
    default:
      decimals = 2;
      step = 0.01;
      break;
  }

}

#define CONNECT(...) m_connectionManager.addConnection(connect(__VA_ARGS__))

void OptionsWidget::setupUiConnections() {
  CONNECT(contentDetectAutoBtn, &QPushButton::pressed, this,
          boost::bind(&OptionsWidget::contentDetectToggled, this, MODE_AUTO));
  CONNECT(contentDetectManualBtn, &QPushButton::pressed, this,
          boost::bind(&OptionsWidget::contentDetectToggled, this, MODE_MANUAL));
  CONNECT(contentDetectDisableBtn, &QPushButton::pressed, this,
          boost::bind(&OptionsWidget::contentDetectToggled, this, MODE_DISABLED));
  CONNECT(applyToBtn, SIGNAL(clicked()), this, SLOT(showApplyToDialog()));
  CONNECT(fillFactorSlider, SIGNAL(valueChanged(int)), this, SLOT(fillFactorChanged(int)));
  CONNECT(borderToleranceSlider, SIGNAL(valueChanged(int)), this, SLOT(borderToleranceChanged(int)));
  // Trigger re-detection when slider is released (not on every value change)
  CONNECT(fillFactorSlider, SIGNAL(sliderReleased()), this, SLOT(detectionSettingsChanged()));
  CONNECT(borderToleranceSlider, SIGNAL(sliderReleased()), this, SLOT(detectionSettingsChanged()));
}

#undef CONNECT

void OptionsWidget::applyModeToSelectedPages() {
  const std::set<PageId> selectedPages = m_pageSelectionAccessor.selectedPages();

  // Only apply to multiple pages if current page is in selection and there are multiple selected
  if (selectedPages.size() > 1 && selectedPages.find(m_pageId) != selectedPages.end()) {
    const Params params(m_uiData.contentRect(), m_uiData.contentSizeMM(), m_uiData.pageRect(), m_uiData.dependencies(),
                        m_uiData.contentDetectionMode(), m_uiData.pageDetectionMode(),
                        m_uiData.isFineTuningCornersEnabled());

    for (const PageId& pageId : selectedPages) {
      if (pageId == m_pageId) {
        continue;
      }

      std::unique_ptr<Params> oldParams = m_settings->getPageParams(pageId);
      if (oldParams) {
        // Copy only the mode settings, not the boxes
        Params newParams(*oldParams);
        newParams.setContentDetectionMode(m_uiData.contentDetectionMode());
        m_settings->setPageParams(pageId, newParams);
      } else {
        m_settings->setPageParams(pageId, params);
      }
    }
    emit invalidateAllThumbnails();
    // Also explicitly invalidate current page to ensure it gets proper thumbnail treatment
    emit invalidateThumbnail(m_pageId);
  }
}

void OptionsWidget::updateSelectionIndicator() {
  const std::set<PageId> selectedPages = m_pageSelectionAccessor.selectedPages();
  if (selectedPages.size() > 1 && selectedPages.find(m_pageId) != selectedPages.end()) {
    selectionIndicatorLabel->setText(tr("Editing %1 pages").arg(selectedPages.size()));
    selectionIndicatorLabel->setStyleSheet("QLabel { color: #4a90d9; font-weight: bold; }");
    selectionIndicatorLabel->show();
  } else {
    selectionIndicatorLabel->hide();
  }
}

void OptionsWidget::updateDetectionSettingsUI() {
  auto block = m_connectionManager.getScopedBlock();

  const double fillFactor = m_settings->contentFillFactor();
  const int borderTolerance = m_settings->borderTolerance();

  fillFactorSlider->setValue(static_cast<int>(fillFactor * 100));
  fillFactorValueLabel->setText(QString::number(fillFactor, 'f', 2));

  borderToleranceSlider->setValue(borderTolerance);
  borderToleranceValueLabel->setText(QString("%1 px").arg(borderTolerance));
}

void OptionsWidget::fillFactorChanged(int value) {
  const double fillFactor = value / 100.0;
  m_settings->setContentFillFactor(fillFactor);
  fillFactorValueLabel->setText(QString::number(fillFactor, 'f', 2));
}

void OptionsWidget::borderToleranceChanged(int value) {
  m_settings->setBorderTolerance(value);
  borderToleranceValueLabel->setText(QString("%1 px").arg(value));
}

void OptionsWidget::detectionSettingsChanged() {
  // Only re-run detection if in Auto mode
  if (m_uiData.contentDetectionMode() == MODE_AUTO) {
    // Clear cached params to force re-detection with new settings
    m_settings->clearPageParams(m_pageId);
    emit reloadRequested();
  }
}


/*========================= OptionsWidget::UiData ======================*/

OptionsWidget::UiData::UiData()
    : m_contentDetectionMode(MODE_AUTO), m_pageDetectionMode(MODE_DISABLED), m_fineTuneCornersEnabled(false) {}

OptionsWidget::UiData::~UiData() = default;
}  // namespace select_content
