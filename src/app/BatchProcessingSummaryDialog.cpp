// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "BatchProcessingSummaryDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>

BatchProcessingSummaryDialog::BatchProcessingSummaryDialog(QWidget* parent)
    : QDialog(parent), m_currentMode(VIEW_NOT_SPLIT) {
  setWindowTitle(tr("Batch Processing Complete"));
  setMinimumWidth(400);
  setMinimumHeight(350);

  auto* mainLayout = new QVBoxLayout(this);

  // Summary label
  m_summaryLabel = new QLabel(this);
  m_summaryLabel->setWordWrap(true);
  QFont summaryFont = m_summaryLabel->font();
  summaryFont.setPointSize(summaryFont.pointSize() + 2);
  m_summaryLabel->setFont(summaryFont);
  mainLayout->addWidget(m_summaryLabel);

  // View toggle buttons
  auto* toggleLayout = new QHBoxLayout();
  m_viewNotSplitButton = new QPushButton(tr("Not Split (0)"), this);
  m_viewNotSplitButton->setCheckable(true);
  m_viewNotSplitButton->setChecked(true);

  m_viewSplitButton = new QPushButton(tr("Split (0)"), this);
  m_viewSplitButton->setCheckable(true);

  m_viewButtonGroup = new QButtonGroup(this);
  m_viewButtonGroup->addButton(m_viewNotSplitButton, VIEW_NOT_SPLIT);
  m_viewButtonGroup->addButton(m_viewSplitButton, VIEW_SPLIT);
  m_viewButtonGroup->setExclusive(true);
  connect(m_viewButtonGroup, QOverload<int>::of(&QButtonGroup::idClicked),
          this, &BatchProcessingSummaryDialog::onViewModeChanged);

  toggleLayout->addWidget(m_viewNotSplitButton);
  toggleLayout->addWidget(m_viewSplitButton);
  toggleLayout->addStretch();
  mainLayout->addLayout(toggleLayout);

  // List label with hint
  m_listLabel = new QLabel(tr("Double-click a page to jump to it:"), this);
  mainLayout->addWidget(m_listLabel);

  // Page list
  m_pagesList = new QListWidget(this);
  m_pagesList->setSelectionMode(QAbstractItemView::ExtendedSelection);
  connect(m_pagesList, &QListWidget::itemDoubleClicked,
          this, &BatchProcessingSummaryDialog::onItemDoubleClicked);
  mainLayout->addWidget(m_pagesList);

  // Button layout - first row
  auto* buttonLayout1 = new QHBoxLayout();

  m_jumpButton = new QPushButton(tr("Jump to Selected"), this);
  m_jumpButton->setEnabled(false);
  connect(m_jumpButton, &QPushButton::clicked,
          this, &BatchProcessingSummaryDialog::onJumpButtonClicked);
  connect(m_pagesList, &QListWidget::itemSelectionChanged, [this]() {
    bool hasSelection = !m_pagesList->selectedItems().isEmpty();
    m_jumpButton->setEnabled(hasSelection);
    m_actionButton->setEnabled(hasSelection);
  });
  buttonLayout1->addWidget(m_jumpButton);

  m_actionButton = new QPushButton(tr("Force Two-Page (Selected)"), this);
  m_actionButton->setEnabled(false);
  connect(m_actionButton, &QPushButton::clicked,
          this, &BatchProcessingSummaryDialog::onActionButtonClicked);
  buttonLayout1->addWidget(m_actionButton);

  buttonLayout1->addStretch();

  mainLayout->addLayout(buttonLayout1);

  // Button layout - second row
  auto* buttonLayout2 = new QHBoxLayout();

  m_actionAllButton = new QPushButton(tr("Force Two-Page (All Listed)"), this);
  connect(m_actionAllButton, &QPushButton::clicked,
          this, &BatchProcessingSummaryDialog::onActionAllButtonClicked);
  buttonLayout2->addWidget(m_actionAllButton);

  buttonLayout2->addStretch();

  m_closeButton = new QPushButton(tr("Close"), this);
  connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
  buttonLayout2->addWidget(m_closeButton);

  mainLayout->addLayout(buttonLayout2);
}

void BatchProcessingSummaryDialog::setSummary(int totalPages, int splitPages, int singlePages,
                                               const std::vector<PageSummary>& singlePageList,
                                               const std::vector<PageSummary>& splitPageList) {
  m_singlePages = singlePageList;
  m_splitPages = splitPageList;

  QString summaryText = tr("Processed %1 images:\n"
                           "  - %2 identified as two-page spreads and split\n"
                           "  - %3 kept as single pages")
                            .arg(totalPages)
                            .arg(splitPages)
                            .arg(singlePages);
  m_summaryLabel->setText(summaryText);

  // Update toggle button labels with counts
  m_viewNotSplitButton->setText(tr("Not Split (%1)").arg(singlePages));
  m_viewSplitButton->setText(tr("Split (%1)").arg(splitPages));

  // Start in the view that has items (prefer not split if both have items)
  if (singlePageList.empty() && !splitPageList.empty()) {
    m_currentMode = VIEW_SPLIT;
    m_viewSplitButton->setChecked(true);
  } else {
    m_currentMode = VIEW_NOT_SPLIT;
    m_viewNotSplitButton->setChecked(true);
  }

  updateListForCurrentMode();
  updateButtonsForCurrentMode();
}

void BatchProcessingSummaryDialog::onViewModeChanged(int mode) {
  m_currentMode = static_cast<ViewMode>(mode);
  updateListForCurrentMode();
  updateButtonsForCurrentMode();
}

void BatchProcessingSummaryDialog::updateListForCurrentMode() {
  m_pagesList->clear();

  const std::vector<PageSummary>& pages = currentPageList();
  for (const auto& page : pages) {
    m_pagesList->addItem(tr("Page %1").arg(page.pageNumber));
  }

  // Update list label - keep the double-click hint consistent
  m_listLabel->setText(tr("Double-click a page to jump to it:"));

  // Show/hide list section based on whether there are items
  bool hasItems = !pages.empty();
  m_pagesList->setVisible(hasItems);
  m_jumpButton->setVisible(hasItems);
  m_actionButton->setVisible(hasItems);
  m_actionAllButton->setVisible(hasItems);
  m_listLabel->setVisible(hasItems);
}

void BatchProcessingSummaryDialog::updateButtonsForCurrentMode() {
  if (m_currentMode == VIEW_NOT_SPLIT) {
    m_actionButton->setText(tr("Force Two-Page (Selected)"));
    m_actionButton->setToolTip(tr("Force the selected pages to be split as two-page spreads"));
    m_actionAllButton->setText(tr("Force Two-Page (All Listed)"));
    m_actionAllButton->setToolTip(tr("Force all listed pages to be split as two-page spreads"));
  } else {
    m_actionButton->setText(tr("Force Single Page (Selected)"));
    m_actionButton->setToolTip(tr("Force the selected pages to be kept as single pages"));
    m_actionAllButton->setText(tr("Force Single Page (All Listed)"));
    m_actionAllButton->setToolTip(tr("Force all listed pages to be kept as single pages"));
  }
}

const std::vector<BatchProcessingSummaryDialog::PageSummary>& BatchProcessingSummaryDialog::currentPageList() const {
  return (m_currentMode == VIEW_NOT_SPLIT) ? m_singlePages : m_splitPages;
}

void BatchProcessingSummaryDialog::onItemDoubleClicked(QListWidgetItem* item) {
  const std::vector<PageSummary>& pages = currentPageList();
  int row = m_pagesList->row(item);
  if (row >= 0 && row < static_cast<int>(pages.size())) {
    // Mark the item with strikethrough to show it's been visited
    QFont font = item->font();
    font.setStrikeOut(true);
    item->setFont(font);
    item->setForeground(Qt::gray);

    emit jumpToPage(pages[row].imageId);
  }
}

void BatchProcessingSummaryDialog::onJumpButtonClicked() {
  const std::vector<PageSummary>& pages = currentPageList();
  int row = m_pagesList->currentRow();
  if (row >= 0 && row < static_cast<int>(pages.size())) {
    // Mark the item with strikethrough to show it's been visited
    QListWidgetItem* item = m_pagesList->item(row);
    if (item) {
      QFont font = item->font();
      font.setStrikeOut(true);
      item->setFont(font);
      item->setForeground(Qt::gray);
    }

    emit jumpToPage(pages[row].imageId);
  }
}

void BatchProcessingSummaryDialog::onActionButtonClicked() {
  const std::vector<PageSummary>& pages = currentPageList();
  QList<QListWidgetItem*> selectedItems = m_pagesList->selectedItems();
  if (selectedItems.isEmpty()) {
    return;
  }

  std::vector<ImageId> selectedImageIds;
  for (QListWidgetItem* item : selectedItems) {
    int row = m_pagesList->row(item);
    if (row >= 0 && row < static_cast<int>(pages.size())) {
      selectedImageIds.push_back(pages[row].imageId);
      // Mark as processed with strikethrough
      QFont font = item->font();
      font.setStrikeOut(true);
      item->setFont(font);
      item->setForeground(Qt::darkGreen);
    }
  }

  if (!selectedImageIds.empty()) {
    if (m_currentMode == VIEW_NOT_SPLIT) {
      emit forceTwoPageSelected(selectedImageIds);
    } else {
      emit forceSinglePageSelected(selectedImageIds);
    }
  }
}

void BatchProcessingSummaryDialog::onActionAllButtonClicked() {
  const std::vector<PageSummary>& pages = currentPageList();
  if (pages.empty()) {
    return;
  }

  std::vector<ImageId> allImageIds;
  for (size_t i = 0; i < pages.size(); ++i) {
    allImageIds.push_back(pages[i].imageId);
    // Mark all as processed with strikethrough
    QListWidgetItem* item = m_pagesList->item(static_cast<int>(i));
    if (item) {
      QFont font = item->font();
      font.setStrikeOut(true);
      item->setFont(font);
      item->setForeground(Qt::darkGreen);
    }
  }

  if (m_currentMode == VIEW_NOT_SPLIT) {
    emit forceTwoPageAll(allImageIds);
  } else {
    emit forceSinglePageAll(allImageIds);
  }
}
