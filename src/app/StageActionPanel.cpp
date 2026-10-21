// Copyright (C) 2026  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "StageActionPanel.h"

#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>

StageActionPanel::StageActionPanel(QWidget* controls, QWidget* parent)
    : QWidget(parent),
      m_heading(new QLabel(this)),
      m_actionRow(new QWidget(this)),
      m_actionLayout(new QGridLayout(m_actionRow)),
      m_primary(new QPushButton(tr("Process all pages"), this)),
      m_secondary(new QPushButton(tr("This page only"), this)),
      m_adjustToggle(new QToolButton(this)),
      m_foldBody(new QWidget(this)),
      m_adjustLayout(new QVBoxLayout(m_foldBody)),
      m_pinnedSlot(new QWidget(this)),
      m_pinnedLayout(new QVBoxLayout(m_pinnedSlot)),
      m_scopeLabel(new QLabel(this)),
      m_waitingLabel(new QLabel(this)) {
  auto* panelLayout = new QVBoxLayout(this);
  panelLayout->setContentsMargins(20, 16, 20, 12);
  panelLayout->setSpacing(12);

  m_heading->setObjectName(QStringLiteral("panelHeading"));
  m_heading->setWordWrap(true);
  QFont headingFont = m_heading->font();
  headingFont.setPointSize(qMax(1, headingFont.pointSize() + 5));
  headingFont.setWeight(QFont::DemiBold);
  m_heading->setFont(headingFont);
  panelLayout->addWidget(m_heading);

  m_actionLayout->setContentsMargins(0, 0, 0, 0);
  m_actionLayout->setHorizontalSpacing(8);
  m_actionLayout->setVerticalSpacing(8);
  m_primary->setObjectName(QStringLiteral("primaryActionButton"));
  m_primary->setMinimumHeight(28);
  m_primary->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  m_secondary->setObjectName(QStringLiteral("secondaryActionButton"));
  m_secondary->setMinimumHeight(28);
  m_secondary->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  m_secondary->setFlat(true);
  m_actionLayout->addWidget(m_primary, 0, 0);
  m_actionLayout->addWidget(m_secondary, 0, 1);
  panelLayout->addWidget(m_actionRow);

  m_adjustToggle->setObjectName(QStringLiteral("adjustToggle"));
  m_adjustToggle->setText(tr("Adjust"));
  m_adjustToggle->setCheckable(true);
  m_adjustToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  m_adjustToggle->setAutoRaise(true);
  panelLayout->addWidget(m_adjustToggle);

  m_adjustLayout->setContentsMargins(0, 0, 0, 0);
  m_adjustLayout->setSpacing(0);
  setAdjustWidget(controls);
  panelLayout->addWidget(m_foldBody);

  m_pinnedLayout->setContentsMargins(0, 0, 0, 0);
  m_pinnedLayout->setSpacing(0);
  panelLayout->addWidget(m_pinnedSlot);

  auto* footerLayout = new QHBoxLayout;
  footerLayout->setContentsMargins(0, 0, 0, 0);
  m_scopeLabel->setObjectName(QStringLiteral("scopeLabel"));
  m_waitingLabel->setObjectName(QStringLiteral("waitingLabel"));
  QFont footerFont = m_scopeLabel->font();
  footerFont.setPointSize(qMax(1, footerFont.pointSize() - 2));
  m_scopeLabel->setFont(footerFont);
  m_waitingLabel->setFont(footerFont);
  footerLayout->addWidget(m_scopeLabel);
  footerLayout->addStretch(1);
  footerLayout->addWidget(m_waitingLabel);
  panelLayout->addLayout(footerLayout);

  connect(m_primary, &QPushButton::clicked, this, [this]() {
    if (m_reviewMode) {
      emit reviewPrimaryRequested();
    } else {
      emit processAllRequested();
    }
  });
  connect(m_secondary, &QPushButton::clicked, this, [this]() {
    if (m_reviewMode) {
      emit reviewSecondaryRequested();
    } else {
      emit thisPageRequested();
    }
  });
  connect(m_adjustToggle, &QToolButton::toggled, this, [this](bool open) {
    m_adjustToggle->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
    m_foldBody->setVisible(open);
    emit adjustToggled(open);
  });

  setHeadingText(QString());
  setPinnedWidget(nullptr);
  setWaitingText(QString());
  setAdjustOpen(true);
}

void StageActionPanel::setPrimaryEnabled(const bool enabled) {
  m_primary->setEnabled(enabled);
}

void StageActionPanel::setHeadingText(const QString& text) {
  m_heading->setText(text);
  m_heading->setVisible(!text.isEmpty());
  updateGeometry();
}

void StageActionPanel::setActionTexts(const QString& primary, const QString& secondary) {
  m_primary->setText(primary);
  m_secondary->setText(secondary);
  m_primary->updateGeometry();
  m_secondary->updateGeometry();
  updateActionLayout();
  updateGeometry();
}

void StageActionPanel::setReviewMode(const bool reviewMode) {
  m_reviewMode = reviewMode;
}

void StageActionPanel::setActionRowVisible(const bool visible) {
  m_actionRow->setVisible(visible);
}

void StageActionPanel::setAdjustWidget(QWidget* widget) {
  if (m_adjustWidget == widget) {
    return;
  }
  if (m_adjustWidget) {
    m_adjustWidget->hide();
    m_adjustLayout->removeWidget(m_adjustWidget);
    m_adjustWidget->setParent(nullptr);
  }
  m_adjustWidget = widget;
  if (m_adjustWidget) {
    m_adjustWidget->setParent(m_foldBody);
    m_adjustLayout->addWidget(m_adjustWidget);
    m_adjustWidget->show();
  }
}

void StageActionPanel::setPinnedWidget(QWidget* widget) {
  if (m_pinnedWidget == widget) {
    m_pinnedSlot->setVisible(widget != nullptr);
    return;
  }
  if (m_pinnedWidget) {
    m_pinnedWidget->hide();
    m_pinnedLayout->removeWidget(m_pinnedWidget);
    m_pinnedWidget->setParent(nullptr);
  }
  m_pinnedWidget = widget;
  if (m_pinnedWidget) {
    m_pinnedWidget->setParent(m_pinnedSlot);
    m_pinnedLayout->addWidget(m_pinnedWidget);
    m_pinnedWidget->show();
  }
  m_pinnedSlot->setVisible(m_pinnedWidget != nullptr);
}

void StageActionPanel::setScopeText(const QString& text) {
  m_scopeLabel->setText(text);
  updateGeometry();
}

void StageActionPanel::setWaitingText(const QString& text) {
  m_waitingLabel->setText(text);
  updateGeometry();
}

void StageActionPanel::setAdjustOpen(const bool open) {
  m_adjustToggle->setChecked(open);
  m_adjustToggle->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
  m_foldBody->setVisible(open);
}

bool StageActionPanel::adjustOpen() const {
  return m_adjustToggle->isChecked();
}

int StageActionPanel::minimumContentWidth() const {
  m_primary->ensurePolished();
  m_secondary->ensurePolished();
  const QMargins margins = layout()->contentsMargins();
  return qMax(m_primary->sizeHint().width(), m_secondary->sizeHint().width()) + margins.left()
         + margins.right();
}

void StageActionPanel::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  updateActionLayout();
}

void StageActionPanel::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  updateActionLayout();
}

void StageActionPanel::updateActionLayout() {
  m_primary->ensurePolished();
  m_secondary->ensurePolished();
  const QMargins margins = layout()->contentsMargins();
  const int availableWidth = contentsRect().width() - margins.left() - margins.right();
  const bool stackActions
      = availableWidth < m_primary->sizeHint().width() + 8 + m_secondary->sizeHint().width();
  if (stackActions == m_actionsStacked) {
    return;
  }

  m_actionLayout->removeWidget(m_primary);
  m_actionLayout->removeWidget(m_secondary);
  if (stackActions) {
    m_actionLayout->addWidget(m_primary, 0, 0, Qt::AlignLeft);
    m_actionLayout->addWidget(m_secondary, 1, 0, Qt::AlignLeft);
  } else {
    m_actionLayout->addWidget(m_primary, 0, 0);
    m_actionLayout->addWidget(m_secondary, 0, 1);
  }
  m_actionsStacked = stackActions;
}
