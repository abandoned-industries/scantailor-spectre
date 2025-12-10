// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "BatchProcessingSummaryDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>

BatchProcessingSummaryDialog::BatchProcessingSummaryDialog(QWidget* parent)
    : QDialog(parent) {
  setWindowTitle(tr("Batch Processing Complete"));
  setMinimumWidth(500);
  setMinimumHeight(350);

  auto* mainLayout = new QVBoxLayout(this);

  // Summary label
  m_summaryLabel = new QLabel(this);
  m_summaryLabel->setWordWrap(true);
  QFont summaryFont = m_summaryLabel->font();
  summaryFont.setPointSize(summaryFont.pointSize() + 2);
  m_summaryLabel->setFont(summaryFont);
  mainLayout->addWidget(m_summaryLabel);

  // List of single pages (not split)
  auto* listLabel = new QLabel(tr("Pages not split (double-click to jump):"), this);
  mainLayout->addWidget(listLabel);

  m_singlePagesList = new QListWidget(this);
  m_singlePagesList->setSelectionMode(QAbstractItemView::ExtendedSelection);
  connect(m_singlePagesList, &QListWidget::itemDoubleClicked,
          this, &BatchProcessingSummaryDialog::onItemDoubleClicked);
  mainLayout->addWidget(m_singlePagesList);

  // Button layout - first row
  auto* buttonLayout1 = new QHBoxLayout();

  m_jumpButton = new QPushButton(tr("Jump to Selected"), this);
  m_jumpButton->setEnabled(false);
  connect(m_jumpButton, &QPushButton::clicked,
          this, &BatchProcessingSummaryDialog::onJumpButtonClicked);
  connect(m_singlePagesList, &QListWidget::itemSelectionChanged, [this]() {
    bool hasSelection = !m_singlePagesList->selectedItems().isEmpty();
    m_jumpButton->setEnabled(hasSelection);
    m_forceTwoPageButton->setEnabled(hasSelection);
  });
  buttonLayout1->addWidget(m_jumpButton);

  m_forceTwoPageButton = new QPushButton(tr("Force Two-Page (Selected)"), this);
  m_forceTwoPageButton->setEnabled(false);
  m_forceTwoPageButton->setToolTip(tr("Force the selected pages to be split as two-page spreads"));
  connect(m_forceTwoPageButton, &QPushButton::clicked,
          this, &BatchProcessingSummaryDialog::onForceTwoPageClicked);
  buttonLayout1->addWidget(m_forceTwoPageButton);

  buttonLayout1->addStretch();

  mainLayout->addLayout(buttonLayout1);

  // Button layout - second row
  auto* buttonLayout2 = new QHBoxLayout();

  m_forceTwoPageAllButton = new QPushButton(tr("Force Two-Page (All Listed)"), this);
  m_forceTwoPageAllButton->setToolTip(tr("Force all listed pages to be split as two-page spreads"));
  connect(m_forceTwoPageAllButton, &QPushButton::clicked,
          this, &BatchProcessingSummaryDialog::onForceTwoPageAllClicked);
  buttonLayout2->addWidget(m_forceTwoPageAllButton);

  buttonLayout2->addStretch();

  m_closeButton = new QPushButton(tr("Close"), this);
  connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
  buttonLayout2->addWidget(m_closeButton);

  mainLayout->addLayout(buttonLayout2);
}

void BatchProcessingSummaryDialog::setSummary(int totalPages, int splitPages, int singlePages,
                                               const std::vector<PageSummary>& singlePageList) {
  m_singlePages = singlePageList;

  QString summaryText = tr("Processed %1 images:\n"
                           "  - %2 identified as two-page spreads and split\n"
                           "  - %3 kept as single pages")
                            .arg(totalPages)
                            .arg(splitPages)
                            .arg(singlePages);
  m_summaryLabel->setText(summaryText);

  m_singlePagesList->clear();
  for (const auto& page : singlePageList) {
    m_singlePagesList->addItem(page.fileName);
  }

  // Hide the list section if there are no single pages
  bool hasSinglePages = !singlePageList.empty();
  m_singlePagesList->setVisible(hasSinglePages);
  m_jumpButton->setVisible(hasSinglePages);
  m_forceTwoPageButton->setVisible(hasSinglePages);
  m_forceTwoPageAllButton->setVisible(hasSinglePages);
}

void BatchProcessingSummaryDialog::onItemDoubleClicked(QListWidgetItem* item) {
  int row = m_singlePagesList->row(item);
  if (row >= 0 && row < static_cast<int>(m_singlePages.size())) {
    // Mark the item with strikethrough to show it's been visited
    QFont font = item->font();
    font.setStrikeOut(true);
    item->setFont(font);
    item->setForeground(Qt::gray);

    emit jumpToPage(m_singlePages[row].imageId);
  }
}

void BatchProcessingSummaryDialog::onJumpButtonClicked() {
  int row = m_singlePagesList->currentRow();
  if (row >= 0 && row < static_cast<int>(m_singlePages.size())) {
    // Mark the item with strikethrough to show it's been visited
    QListWidgetItem* item = m_singlePagesList->item(row);
    if (item) {
      QFont font = item->font();
      font.setStrikeOut(true);
      item->setFont(font);
      item->setForeground(Qt::gray);
    }

    emit jumpToPage(m_singlePages[row].imageId);
  }
}

void BatchProcessingSummaryDialog::onForceTwoPageClicked() {
  QList<QListWidgetItem*> selectedItems = m_singlePagesList->selectedItems();
  if (selectedItems.isEmpty()) {
    return;
  }

  std::vector<ImageId> selectedImageIds;
  for (QListWidgetItem* item : selectedItems) {
    int row = m_singlePagesList->row(item);
    if (row >= 0 && row < static_cast<int>(m_singlePages.size())) {
      selectedImageIds.push_back(m_singlePages[row].imageId);
      // Mark as processed with strikethrough
      QFont font = item->font();
      font.setStrikeOut(true);
      item->setFont(font);
      item->setForeground(Qt::darkGreen);
    }
  }

  if (!selectedImageIds.empty()) {
    emit forceTwoPageSelected(selectedImageIds);
  }
}

void BatchProcessingSummaryDialog::onForceTwoPageAllClicked() {
  if (m_singlePages.empty()) {
    return;
  }

  std::vector<ImageId> allImageIds;
  for (size_t i = 0; i < m_singlePages.size(); ++i) {
    allImageIds.push_back(m_singlePages[i].imageId);
    // Mark all as processed with strikethrough
    QListWidgetItem* item = m_singlePagesList->item(static_cast<int>(i));
    if (item) {
      QFont font = item->font();
      font.setStrikeOut(true);
      item->setFont(font);
      item->setForeground(Qt::darkGreen);
    }
  }

  emit forceTwoPageAll(allImageIds);
}
