// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "StatusBarPanel.h"

#include <core/IconProvider.h>

#include <QtCore/QFileInfo>
#include <cmath>

#include "ImageViewInfoProvider.h"
#include "PageId.h"
#include "UnitsProvider.h"

StatusBarPanel::StatusBarPanel() {
  ui.setupUi(this);
}

void StatusBarPanel::onMousePosChanged(const QPointF& mousePos) {
  StatusBarPanel::m_mousePos = mousePos;
  mousePosChanged();
}

void StatusBarPanel::onPhysSizeChanged(const QSizeF& physSize) {
  StatusBarPanel::m_physSize = physSize;
  physSizeChanged();
}

void StatusBarPanel::onDpiChanged(const Dpi& dpi) {
  StatusBarPanel::m_dpi = dpi;
}

void StatusBarPanel::onImageViewInfoProviderStopped() {
  onMousePosChanged(QPointF());
  onPhysSizeChanged(QRectF().size());
  m_dpi = Dpi();
}

void StatusBarPanel::updatePage(int pageNumber, size_t pageCount, const PageId& pageId) {
  // Page info display removed - redundant with thumbnail labels showing filename + L/R suffix
  (void)pageNumber;
  (void)pageCount;
  (void)pageId;

  ui.pageInfoLine->setVisible(false);
  ui.pageInfoLabel->setVisible(false);
}

namespace {
inline void clearAndHideLabel(QLabel* widget) {
  widget->clear();
  widget->hide();
}
}  // namespace

void StatusBarPanel::clear() {
  clearAndHideLabel(ui.mousePosLabel);
  clearAndHideLabel(ui.physSizeLabel);
  clearAndHideLabel(ui.pageInfoLabel);
  clearAndHideLabel(ui.zoneModeLabel);

  ui.mousePosLine->setVisible(false);
  ui.physSizeLine->setVisible(false);
  ui.pageInfoLine->setVisible(false);
  ui.zoneModeLine->setVisible(false);
}

void StatusBarPanel::onUnitsChanged(Units) {
  mousePosChanged();
  physSizeChanged();
}

void StatusBarPanel::mousePosChanged() {
  if (!m_mousePos.isNull() && !m_dpi.isNull()) {
    double x = m_mousePos.x();
    double y = m_mousePos.y();
    UnitsProvider::getInstance().convertFrom(x, y, PIXELS, m_dpi);

    switch (UnitsProvider::getInstance().getUnits()) {
      case PIXELS:
      case MILLIMETRES:
        x = std::ceil(x);
        y = std::ceil(y);
        break;
      default:
        x = std::ceil(x * 10) / 10;
        y = std::ceil(y * 10) / 10;
        break;
    }

    ui.mousePosLine->setVisible(true);
    ui.mousePosLabel->setText(QString("%1, %2").arg(x).arg(y));
    ui.mousePosLabel->setVisible(true);
  } else {
    clearAndHideLabel(ui.mousePosLabel);
    ui.mousePosLine->setVisible(false);
  }
}

void StatusBarPanel::physSizeChanged() {
  if (!m_physSize.isNull() && !m_dpi.isNull()) {
    double width = m_physSize.width();
    double height = m_physSize.height();
    UnitsProvider::getInstance().convertFrom(width, height, PIXELS, m_dpi);

    const Units units = UnitsProvider::getInstance().getUnits();
    switch (units) {
      case PIXELS:
        width = std::round(width);
        height = std::round(height);
        break;
      case MILLIMETRES:
        width = std::round(width);
        height = std::round(height);
        break;
      case CENTIMETRES:
        width = std::round(width * 10) / 10;
        height = std::round(height * 10) / 10;
        break;
      case INCHES:
        width = std::round(width * 10) / 10;
        height = std::round(height * 10) / 10;
        break;
    }

    ui.physSizeLine->setVisible(true);
    ui.physSizeLabel->setText(QString("%1 x %2 %3").arg(width).arg(height).arg(unitsToLocalizedString(units)));
    ui.physSizeLabel->setVisible(true);
  } else {
    clearAndHideLabel(ui.physSizeLabel);
    ui.physSizeLine->setVisible(false);
  }
}

void StatusBarPanel::onZoneModeChanged(ZoneCreationMode mode) {
  switch (mode) {
    case ZoneCreationMode::RECTANGULAR:
      ui.zoneModeLabel->setPixmap(IconProvider::getInstance().getIcon("rectangular-zone-mode").pixmap(16, 16));
      break;
    case ZoneCreationMode::LASSO:
      ui.zoneModeLabel->setPixmap(IconProvider::getInstance().getIcon("lasso-zone-mode").pixmap(16, 16));
      break;
    case ZoneCreationMode::POLYGONAL:
      ui.zoneModeLabel->setPixmap(IconProvider::getInstance().getIcon("polygonal-zone-mode").pixmap(16, 16));
      break;
  }
  ui.zoneModeLabel->setVisible(true);
  ui.zoneModeLine->setVisible(true);
}

void StatusBarPanel::onZoneModeProviderStopped() {
  clearAndHideLabel(ui.zoneModeLabel);
  ui.zoneModeLine->setVisible(false);
}
