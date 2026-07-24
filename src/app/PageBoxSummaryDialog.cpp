// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "PageBoxSummaryDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>
#include <algorithm>
#include <cmath>

PageBoxSummaryDialog::PageBoxSummaryDialog(QWidget* parent)
    : QDialog(parent), m_currentMode(VIEW_OUTLIER_PAGES), m_thresholdPercent(10), m_medianWidth(0.0) {
  setWindowTitle(tr("Page Box Detection Complete"));
  setMinimumWidth(500);
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
  m_thresholdLabel = new QLabel(tr("Deviation threshold: 10%"), this);
  thresholdLayout->addWidget(m_thresholdLabel);

  m_thresholdSlider = new QSlider(Qt::Horizontal, this);
  m_thresholdSlider->setRange(5, 30);  // 5% to 30%
  m_thresholdSlider->setValue(10);
  m_thresholdSlider->setTickPosition(QSlider::NoTicks);
  m_thresholdSlider->setTickInterval(5);
  connect(m_thresholdSlider, &QSlider::valueChanged,
          this, &PageBoxSummaryDialog::onThresholdChanged);
  thresholdLayout->addWidget(m_thresholdSlider);

  mainLayout->addLayout(thresholdLayout);

  // View toggle buttons
  auto* toggleLayout = new QHBoxLayout();
  m_viewOutlierButton = new QPushButton(tr("Outlier Pages (0)"), this);
  m_viewOutlierButton->setCheckable(true);
  m_viewOutlierButton->setChecked(true);

  m_viewNormalButton = new QPushButton(tr("Normal Pages (0)"), this);
  m_viewNormalButton->setCheckable(true);

  m_viewButtonGroup = new QButtonGroup(this);
  m_viewButtonGroup->addButton(m_viewOutlierButton, VIEW_OUTLIER_PAGES);
  m_viewButtonGroup->addButton(m_viewNormalButton, VIEW_NORMAL_PAGES);
  m_viewButtonGroup->setExclusive(true);
  connect(m_viewButtonGroup, QOverload<int>::of(&QButtonGroup::idClicked),
          this, &PageBoxSummaryDialog::onViewModeChanged);

  toggleLayout->addWidget(m_viewOutlierButton);
  toggleLayout->addWidget(m_viewNormalButton);
  toggleLayout->addStretch();
  mainLayout->addLayout(toggleLayout);

  // List label with hint
  m_listLabel = new QLabel(tr("Double-click a page to jump to it:"), this);
  mainLayout->addWidget(m_listLabel);

  // Page list
  m_pagesList = new QListWidget(this);
  m_pagesList->setSelectionMode(QAbstractItemView::ExtendedSelection);
  connect(m_pagesList, &QListWidget::itemDoubleClicked,
          this, &PageBoxSummaryDialog::onItemDoubleClicked);
  mainLayout->addWidget(m_pagesList);

  // Button layout - first row
  auto* buttonLayout1 = new QHBoxLayout();

  m_jumpButton = new QPushButton(tr("Jump to Selected"), this);
  m_jumpButton->setEnabled(false);
  connect(m_jumpButton, &QPushButton::clicked,
          this, &PageBoxSummaryDialog::onJumpButtonClicked);
  connect(m_pagesList, &QListWidget::itemSelectionChanged, [this]() {
    bool hasSelection = !m_pagesList->selectedItems().isEmpty();
    m_jumpButton->setEnabled(hasSelection);
    m_actionButton->setEnabled(hasSelection && m_currentMode == VIEW_OUTLIER_PAGES);
  });
  buttonLayout1->addWidget(m_jumpButton);

  m_actionButton = new QPushButton(tr("Disable Page Box (Selected)"), this);
  m_actionButton->setEnabled(false);
  m_actionButton->setToolTip(tr("Set page detection mode to DISABLED for selected outlier pages"));
  connect(m_actionButton, &QPushButton::clicked,
          this, &PageBoxSummaryDialog::onActionButtonClicked);
  buttonLayout1->addWidget(m_actionButton);

  buttonLayout1->addStretch();

  mainLayout->addLayout(buttonLayout1);

  // Button layout - second row
  auto* buttonLayout2 = new QHBoxLayout();

  m_actionAllButton = new QPushButton(tr("Disable Page Box (All Listed)"), this);
  m_actionAllButton->setToolTip(tr("Set page detection mode to DISABLED for all listed outlier pages"));
  connect(m_actionAllButton, &QPushButton::clicked,
          this, &PageBoxSummaryDialog::onActionAllButtonClicked);
  buttonLayout2->addWidget(m_actionAllButton);

  buttonLayout2->addStretch();

  m_closeButton = new QPushButton(tr("Close"), this);
  connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
  buttonLayout2->addWidget(m_closeButton);

  mainLayout->addLayout(buttonLayout2);
}

void PageBoxSummaryDialog::setSummary(int totalPages,
                                       const std::vector<PageSummary>& allPages,
                                       int initialThresholdPercent) {
  m_allPages = allPages;
  m_thresholdPercent = initialThresholdPercent;
  m_thresholdSlider->setValue(m_thresholdPercent);
  m_medianWidth = computeMedianWidth();

  // Recompute deviation percentages based on median
  for (auto& page : m_allPages) {
    if (m_medianWidth > 0.0) {
      page.deviationPercent = std::abs(page.pageWidth - m_medianWidth) / m_medianWidth * 100.0;
    } else {
      page.deviationPercent = 0.0;
    }
  }

  updateListsForThreshold();

  QString summaryText = tr("Processed %1 pages (median width: %2px):\n"
                           "  - %3 outlier pages (width deviation > %4%)\n"
                           "  - %5 normal pages")
                            .arg(totalPages)
                            .arg(static_cast<int>(m_medianWidth))
                            .arg(m_outlierPages.size())
                            .arg(m_thresholdPercent)
                            .arg(m_normalPages.size());
  m_summaryLabel->setText(summaryText);

  // Start in the view that has items (prefer outliers if both have items)
  if (m_outlierPages.empty() && !m_normalPages.empty()) {
    m_currentMode = VIEW_NORMAL_PAGES;
    m_viewNormalButton->setChecked(true);
  } else {
    m_currentMode = VIEW_OUTLIER_PAGES;
    m_viewOutlierButton->setChecked(true);
  }

  updateListForCurrentMode();
  updateButtonsForCurrentMode();
}

double PageBoxSummaryDialog::computeMedianWidth() const {
  if (m_allPages.empty()) {
    return 0.0;
  }

  std::vector<double> widths;
  widths.reserve(m_allPages.size());
  for (const auto& page : m_allPages) {
    widths.push_back(page.pageWidth);
  }
  std::sort(widths.begin(), widths.end());

  size_t n = widths.size();
  if (n % 2 == 0) {
    return (widths[n / 2 - 1] + widths[n / 2]) / 2.0;
  } else {
    return widths[n / 2];
  }
}

void PageBoxSummaryDialog::onThresholdChanged(int value) {
  m_thresholdPercent = value;
  m_thresholdLabel->setText(tr("Deviation threshold: %1%").arg(value));

  updateListsForThreshold();

  // Update summary text
  QString summaryText = tr("Processed %1 pages (median width: %2px):\n"
                           "  - %3 outlier pages (width deviation > %4%)\n"
                           "  - %5 normal pages")
                            .arg(m_allPages.size())
                            .arg(static_cast<int>(m_medianWidth))
                            .arg(m_outlierPages.size())
                            .arg(m_thresholdPercent)
                            .arg(m_normalPages.size());
  m_summaryLabel->setText(summaryText);

  updateListForCurrentMode();
  updateButtonsForCurrentMode();
}

void PageBoxSummaryDialog::updateListsForThreshold() {
  m_outlierPages.clear();
  m_normalPages.clear();

  for (const auto& page : m_allPages) {
    if (page.deviationPercent > static_cast<double>(m_thresholdPercent)) {
      m_outlierPages.push_back(page);
    } else {
      m_normalPages.push_back(page);
    }
  }

  // Update toggle button labels with counts
  m_viewOutlierButton->setText(tr("Outlier Pages (%1)").arg(m_outlierPages.size()));
  m_viewNormalButton->setText(tr("Normal Pages (%1)").arg(m_normalPages.size()));
}

void PageBoxSummaryDialog::onViewModeChanged(int mode) {
  m_currentMode = static_cast<ViewMode>(mode);
  updateListForCurrentMode();
  updateButtonsForCurrentMode();
}

void PageBoxSummaryDialog::updateListForCurrentMode() {
  m_pagesList->clear();

  const std::vector<PageSummary>& pages = currentPageList();
  for (const auto& page : pages) {
    m_pagesList->addItem(tr("Page %1 (width: %2px, deviation: %3%)")
                             .arg(page.pageNumber)
                             .arg(static_cast<int>(page.pageWidth))
                             .arg(QString::number(page.deviationPercent, 'f', 1)));
  }

  // Update list label - keep the double-click hint consistent
  m_listLabel->setText(tr("Double-click a page to jump to it:"));

  // Show/hide list section based on whether there are items
  bool hasItems = !pages.empty();
  m_pagesList->setVisible(hasItems);
  m_jumpButton->setVisible(hasItems);
  m_actionButton->setVisible(hasItems && m_currentMode == VIEW_OUTLIER_PAGES);
  m_actionAllButton->setVisible(hasItems && m_currentMode == VIEW_OUTLIER_PAGES);
  m_listLabel->setVisible(hasItems);
}

void PageBoxSummaryDialog::updateButtonsForCurrentMode() {
  // Action buttons only make sense for outlier pages
  bool showActions = (m_currentMode == VIEW_OUTLIER_PAGES);
  m_actionButton->setVisible(showActions);
  m_actionAllButton->setVisible(showActions);

  if (showActions) {
    m_actionButton->setText(tr("Disable Page Box (Selected)"));
    m_actionButton->setToolTip(tr("Set page detection mode to DISABLED for selected outlier pages"));
    m_actionAllButton->setText(tr("Disable Page Box (All Listed)"));
    m_actionAllButton->setToolTip(tr("Set page detection mode to DISABLED for all listed outlier pages"));
  }
}

const std::vector<PageBoxSummaryDialog::PageSummary>& PageBoxSummaryDialog::currentPageList() const {
  return (m_currentMode == VIEW_OUTLIER_PAGES) ? m_outlierPages : m_normalPages;
}

void PageBoxSummaryDialog::onItemDoubleClicked(QListWidgetItem* item) {
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

void PageBoxSummaryDialog::onJumpButtonClicked() {
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

void PageBoxSummaryDialog::onActionButtonClicked() {
  if (m_currentMode != VIEW_OUTLIER_PAGES) {
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
    emit disablePageBoxSelected(selectedPageIds);
  }
}

void PageBoxSummaryDialog::onActionAllButtonClicked() {
  if (m_currentMode != VIEW_OUTLIER_PAGES) {
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

  emit disablePageBoxAll(allPageIds);
}
