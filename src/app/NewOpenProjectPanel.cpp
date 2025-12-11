// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "NewOpenProjectPanel.h"

#include <QFileInfo>
#include <QMouseEvent>
#include <QVBoxLayout>

#include "RecentProjects.h"

// ClickableLabel implementation
ClickableLabel::ClickableLabel(const QString& text, QWidget* parent) : QLabel(text, parent) {
  setCursor(Qt::PointingHandCursor);
  m_normalColor = palette().color(QPalette::WindowText);
  m_hoverColor = palette().color(QPalette::Link);
  applyColor(m_normalColor);
}

void ClickableLabel::setTextColor(const QColor& color) {
  m_normalColor = color;
  applyColor(m_normalColor);
}

void ClickableLabel::applyColor(const QColor& color) {
  QPalette pal = palette();
  pal.setColor(QPalette::WindowText, color);
  setPalette(pal);
}

void ClickableLabel::enterEvent(QEnterEvent* event) {
  // No color change on hover - cursor change only
  QLabel::enterEvent(event);
}

void ClickableLabel::leaveEvent(QEvent* event) {
  QLabel::leaveEvent(event);
}

void ClickableLabel::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    emit clicked();
  }
  QLabel::mousePressEvent(event);
}

// NewOpenProjectPanel implementation
NewOpenProjectPanel::NewOpenProjectPanel(QWidget* parent) : QWidget(parent) {
  // Main layout - anchored top-left with fixed margins
  auto* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(40, 40, 40, 40);
  mainLayout->setSpacing(8);
  mainLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);

  // Action labels - bold weight
  QFont actionFont;
  actionFont.setPointSize(15);
  actionFont.setWeight(QFont::DemiBold);

  auto* importPdfLabel = new ClickableLabel(tr("Import PDF"), this);
  importPdfLabel->setFont(actionFont);
  mainLayout->addWidget(importPdfLabel);

  auto* importFolderLabel = new ClickableLabel(tr("Import Folder"), this);
  importFolderLabel->setFont(actionFont);
  mainLayout->addWidget(importFolderLabel);

  auto* openProjectLabel = new ClickableLabel(tr("Open Project"), this);
  openProjectLabel->setFont(actionFont);
  mainLayout->addWidget(openProjectLabel);

  // Spacer between actions and recent section
  mainLayout->addSpacing(20);

  // Recent section label - regular weight, secondary color
  QFont sectionFont;
  sectionFont.setPointSize(13);

  auto* recentLabel = new QLabel(tr("Recent"), this);
  recentLabel->setFont(sectionFont);
  QPalette recentPal = recentLabel->palette();
  recentPal.setColor(QPalette::WindowText, palette().color(QPalette::PlaceholderText));
  recentLabel->setPalette(recentPal);
  mainLayout->addWidget(recentLabel);

  // Recent projects container
  m_recentContainer = new QWidget(this);
  m_recentLayout = new QVBoxLayout(m_recentContainer);
  m_recentLayout->setContentsMargins(0, 0, 0, 0);
  m_recentLayout->setSpacing(4);
  mainLayout->addWidget(m_recentContainer);

  // "No recent projects" label - tertiary color
  m_noRecentLabel = new QLabel(tr("No recent projects"), m_recentContainer);
  m_noRecentLabel->setFont(sectionFont);
  QPalette noRecentPal = m_noRecentLabel->palette();
  noRecentPal.setColor(QPalette::WindowText, palette().color(QPalette::Disabled, QPalette::WindowText));
  m_noRecentLabel->setPalette(noRecentPal);
  m_recentLayout->addWidget(m_noRecentLabel);

  // Push everything to the top
  mainLayout->addStretch();

  // Load recent projects
  RecentProjects rp;
  rp.read();
  if (!rp.validate()) {
    rp.write();
  }

  if (!rp.isEmpty()) {
    m_noRecentLabel->setVisible(false);
    rp.enumerate([this](const QString& filePath) { addRecentProject(filePath); });
  }

  // Connect signals
  connect(importPdfLabel, &ClickableLabel::clicked, this, &NewOpenProjectPanel::importPdf);
  connect(importFolderLabel, &ClickableLabel::clicked, this, &NewOpenProjectPanel::importFolder);
  connect(openProjectLabel, &ClickableLabel::clicked, this, &NewOpenProjectPanel::openProject);
}

void NewOpenProjectPanel::addRecentProject(const QString& filePath) {
  const QFileInfo fileInfo(filePath);
  QString baseName(fileInfo.completeBaseName());
  if (baseName.isEmpty()) {
    baseName = QChar('_');
  }

  auto* label = new ClickableLabel(baseName, m_recentContainer);
  label->setToolTip(filePath);

  QFont font;
  font.setPointSize(13);
  label->setFont(font);

  // Secondary color for recent items
  label->setTextColor(palette().color(QPalette::PlaceholderText));

  m_recentLayout->addWidget(label);

  connect(label, &ClickableLabel::clicked, this, [this, filePath]() {
    emit openRecentProject(filePath);
  });
}
