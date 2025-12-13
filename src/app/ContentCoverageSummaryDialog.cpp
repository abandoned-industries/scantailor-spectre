// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "ContentCoverageSummaryDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>

ContentCoverageSummaryDialog::ContentCoverageSummaryDialog(QWidget* parent)
    : QDialog(parent), m_currentMode(VIEW_LOW_COVERAGE), m_threshold(0.5) {
  setWindowTitle(tr("Content Detection Complete"));
  setMinimumWidth(450);
  setMinimumHeight(400);

  auto* mainLayout = new QVBoxLayout(this);

  // Summary label
  m_summaryLabel = new QLabel(this);
  m_summaryLabel->setWordWrap(true);
  QFont summaryFont = m_summaryLabel->font();
  summaryFont.setPointSize(summaryFont.pointSize() + 2);
  m_summaryLabel->setFont(summaryFont);
  mainLayout->addWidget(m_summaryLabel);

  // Threshold slider
  auto* thresholdLayout = new QHBoxLayout();
  m_thresholdLabel = new QLabel(tr("Coverage threshold: 50%"), this);
  thresholdLayout->addWidget(m_thresholdLabel);

  m_thresholdSlider = new QSlider(Qt::Horizontal, this);
  m_thresholdSlider->setRange(20, 80);  // 20% to 80%
  m_thresholdSlider->setValue(50);
  m_thresholdSlider->setTickPosition(QSlider::TicksBelow);
  m_thresholdSlider->setTickInterval(10);
  connect(m_thresholdSlider, &QSlider::valueChanged,
          this, &ContentCoverageSummaryDialog::onThresholdChanged);
  thresholdLayout->addWidget(m_thresholdSlider);

  mainLayout->addLayout(thresholdLayout);

  // View toggle buttons
  auto* toggleLayout = new QHBoxLayout();
  m_viewLowCoverageButton = new QPushButton(tr("Low Coverage (0)"), this);
  m_viewLowCoverageButton->setCheckable(true);
  m_viewLowCoverageButton->setChecked(true);

  m_viewNormalCoverageButton = new QPushButton(tr("Normal Coverage (0)"), this);
  m_viewNormalCoverageButton->setCheckable(true);

  m_viewButtonGroup = new QButtonGroup(this);
  m_viewButtonGroup->addButton(m_viewLowCoverageButton, VIEW_LOW_COVERAGE);
  m_viewButtonGroup->addButton(m_viewNormalCoverageButton, VIEW_NORMAL_COVERAGE);
  m_viewButtonGroup->setExclusive(true);
  connect(m_viewButtonGroup, QOverload<int>::of(&QButtonGroup::idClicked),
          this, &ContentCoverageSummaryDialog::onViewModeChanged);

  toggleLayout->addWidget(m_viewLowCoverageButton);
  toggleLayout->addWidget(m_viewNormalCoverageButton);
  toggleLayout->addStretch();
  mainLayout->addLayout(toggleLayout);

  // List label with hint
  m_listLabel = new QLabel(tr("Double-click a page to jump to it:"), this);
  mainLayout->addWidget(m_listLabel);

  // Page list
  m_pagesList = new QListWidget(this);
  m_pagesList->setSelectionMode(QAbstractItemView::ExtendedSelection);
  connect(m_pagesList, &QListWidget::itemDoubleClicked,
          this, &ContentCoverageSummaryDialog::onItemDoubleClicked);
  mainLayout->addWidget(m_pagesList);

  // Button layout - first row
  auto* buttonLayout1 = new QHBoxLayout();

  m_jumpButton = new QPushButton(tr("Jump to Selected"), this);
  m_jumpButton->setEnabled(false);
  connect(m_jumpButton, &QPushButton::clicked,
          this, &ContentCoverageSummaryDialog::onJumpButtonClicked);
  connect(m_pagesList, &QListWidget::itemSelectionChanged, [this]() {
    bool hasSelection = !m_pagesList->selectedItems().isEmpty();
    m_jumpButton->setEnabled(hasSelection);
    m_actionButton->setEnabled(hasSelection && m_currentMode == VIEW_LOW_COVERAGE);
  });
  buttonLayout1->addWidget(m_jumpButton);

  m_actionButton = new QPushButton(tr("Preserve Layout (Selected)"), this);
  m_actionButton->setEnabled(false);
  m_actionButton->setToolTip(tr("Disable content detection for selected pages to preserve their original layout"));
  connect(m_actionButton, &QPushButton::clicked,
          this, &ContentCoverageSummaryDialog::onActionButtonClicked);
  buttonLayout1->addWidget(m_actionButton);

  buttonLayout1->addStretch();

  mainLayout->addLayout(buttonLayout1);

  // Button layout - second row
  auto* buttonLayout2 = new QHBoxLayout();

  m_actionAllButton = new QPushButton(tr("Preserve Layout (All Listed)"), this);
  m_actionAllButton->setToolTip(tr("Disable content detection for all listed pages to preserve their original layout"));
  connect(m_actionAllButton, &QPushButton::clicked,
          this, &ContentCoverageSummaryDialog::onActionAllButtonClicked);
  buttonLayout2->addWidget(m_actionAllButton);

  buttonLayout2->addStretch();

  m_closeButton = new QPushButton(tr("Close"), this);
  connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
  buttonLayout2->addWidget(m_closeButton);

  mainLayout->addLayout(buttonLayout2);
}

void ContentCoverageSummaryDialog::setSummary(int totalPages,
                                               const std::vector<PageSummary>& allPages,
                                               double initialThreshold) {
  m_allPages = allPages;
  m_threshold = initialThreshold;
  m_thresholdSlider->setValue(static_cast<int>(m_threshold * 100));

  updateListsForThreshold();

  QString summaryText = tr("Processed %1 pages:\n"
                           "  - %2 pages with low content coverage (below %3%)\n"
                           "  - %4 pages with normal content coverage")
                            .arg(totalPages)
                            .arg(m_lowCoveragePages.size())
                            .arg(static_cast<int>(m_threshold * 100))
                            .arg(m_normalCoveragePages.size());
  m_summaryLabel->setText(summaryText);

  // Start in the view that has items (prefer low coverage if both have items)
  if (m_lowCoveragePages.empty() && !m_normalCoveragePages.empty()) {
    m_currentMode = VIEW_NORMAL_COVERAGE;
    m_viewNormalCoverageButton->setChecked(true);
  } else {
    m_currentMode = VIEW_LOW_COVERAGE;
    m_viewLowCoverageButton->setChecked(true);
  }

  updateListForCurrentMode();
  updateButtonsForCurrentMode();
}

void ContentCoverageSummaryDialog::onThresholdChanged(int value) {
  m_threshold = value / 100.0;
  m_thresholdLabel->setText(tr("Coverage threshold: %1%").arg(value));

  updateListsForThreshold();

  // Update summary text
  QString summaryText = tr("Processed %1 pages:\n"
                           "  - %2 pages with low content coverage (below %3%)\n"
                           "  - %4 pages with normal content coverage")
                            .arg(m_allPages.size())
                            .arg(m_lowCoveragePages.size())
                            .arg(static_cast<int>(m_threshold * 100))
                            .arg(m_normalCoveragePages.size());
  m_summaryLabel->setText(summaryText);

  updateListForCurrentMode();
  updateButtonsForCurrentMode();
}

void ContentCoverageSummaryDialog::updateListsForThreshold() {
  m_lowCoveragePages.clear();
  m_normalCoveragePages.clear();

  for (const auto& page : m_allPages) {
    if (page.coverageRatio < m_threshold) {
      m_lowCoveragePages.push_back(page);
    } else {
      m_normalCoveragePages.push_back(page);
    }
  }

  // Update toggle button labels with counts
  m_viewLowCoverageButton->setText(tr("Low Coverage (%1)").arg(m_lowCoveragePages.size()));
  m_viewNormalCoverageButton->setText(tr("Normal Coverage (%1)").arg(m_normalCoveragePages.size()));
}

void ContentCoverageSummaryDialog::onViewModeChanged(int mode) {
  m_currentMode = static_cast<ViewMode>(mode);
  updateListForCurrentMode();
  updateButtonsForCurrentMode();
}

void ContentCoverageSummaryDialog::updateListForCurrentMode() {
  m_pagesList->clear();

  const std::vector<PageSummary>& pages = currentPageList();
  for (const auto& page : pages) {
    int coveragePercent = static_cast<int>(page.coverageRatio * 100);
    m_pagesList->addItem(tr("Page %1 (%2% coverage)").arg(page.pageNumber).arg(coveragePercent));
  }

  // Update list label - keep the double-click hint consistent
  m_listLabel->setText(tr("Double-click a page to jump to it:"));

  // Show/hide list section based on whether there are items
  bool hasItems = !pages.empty();
  m_pagesList->setVisible(hasItems);
  m_jumpButton->setVisible(hasItems);
  m_actionButton->setVisible(hasItems && m_currentMode == VIEW_LOW_COVERAGE);
  m_actionAllButton->setVisible(hasItems && m_currentMode == VIEW_LOW_COVERAGE);
  m_listLabel->setVisible(hasItems);
}

void ContentCoverageSummaryDialog::updateButtonsForCurrentMode() {
  // Action buttons only make sense for low coverage pages
  bool showActions = (m_currentMode == VIEW_LOW_COVERAGE);
  m_actionButton->setVisible(showActions);
  m_actionAllButton->setVisible(showActions);

  if (showActions) {
    m_actionButton->setText(tr("Preserve Layout (Selected)"));
    m_actionButton->setToolTip(tr("Disable content detection for selected pages to preserve their original layout"));
    m_actionAllButton->setText(tr("Preserve Layout (All Listed)"));
    m_actionAllButton->setToolTip(tr("Disable content detection for all listed pages to preserve their original layout"));
  }
}

const std::vector<ContentCoverageSummaryDialog::PageSummary>& ContentCoverageSummaryDialog::currentPageList() const {
  return (m_currentMode == VIEW_LOW_COVERAGE) ? m_lowCoveragePages : m_normalCoveragePages;
}

void ContentCoverageSummaryDialog::onItemDoubleClicked(QListWidgetItem* item) {
  const std::vector<PageSummary>& pages = currentPageList();
  int row = m_pagesList->row(item);
  if (row >= 0 && row < static_cast<int>(pages.size())) {
    // Mark the item with strikethrough to show it's been visited
    QFont font = item->font();
    font.setStrikeOut(true);
    item->setFont(font);
    item->setForeground(Qt::gray);

    emit jumpToPage(pages[row].pageId);
  }
}

void ContentCoverageSummaryDialog::onJumpButtonClicked() {
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

    emit jumpToPage(pages[row].pageId);
  }
}

void ContentCoverageSummaryDialog::onActionButtonClicked() {
  if (m_currentMode != VIEW_LOW_COVERAGE) {
    return;
  }

  const std::vector<PageSummary>& pages = currentPageList();
  QList<QListWidgetItem*> selectedItems = m_pagesList->selectedItems();
  if (selectedItems.isEmpty()) {
    return;
  }

  std::vector<PageId> selectedPageIds;
  for (QListWidgetItem* item : selectedItems) {
    int row = m_pagesList->row(item);
    if (row >= 0 && row < static_cast<int>(pages.size())) {
      selectedPageIds.push_back(pages[row].pageId);
      // Mark as processed with strikethrough
      QFont font = item->font();
      font.setStrikeOut(true);
      item->setFont(font);
      item->setForeground(Qt::darkGreen);
    }
  }

  if (!selectedPageIds.empty()) {
    emit preserveLayoutSelected(selectedPageIds);
  }
}

void ContentCoverageSummaryDialog::onActionAllButtonClicked() {
  if (m_currentMode != VIEW_LOW_COVERAGE) {
    return;
  }

  const std::vector<PageSummary>& pages = currentPageList();
  if (pages.empty()) {
    return;
  }

  std::vector<PageId> allPageIds;
  for (size_t i = 0; i < pages.size(); ++i) {
    allPageIds.push_back(pages[i].pageId);
    // Mark all as processed with strikethrough
    QListWidgetItem* item = m_pagesList->item(static_cast<int>(i));
    if (item) {
      QFont font = item->font();
      font.setStrikeOut(true);
      item->setFont(font);
      item->setForeground(Qt::darkGreen);
    }
  }

  emit preserveLayoutAll(allPageIds);
}
