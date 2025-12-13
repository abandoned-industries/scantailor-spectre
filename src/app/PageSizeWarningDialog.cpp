// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "PageSizeWarningDialog.h"

#include <QDebug>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>

PageSizeWarningDialog::PageSizeWarningDialog(QWidget* parent)
    : QDialog(parent),
      m_medianWidthMM(0),
      m_medianHeightMM(0),
      m_aggregateWidthMM(0),
      m_aggregateHeightMM(0),
      m_threshold(1.3),
      m_totalPages(0),
      m_isSpreadMode(false) {
  setWindowTitle(tr("Page Size Issue"));
  setMinimumWidth(550);
  setMinimumHeight(500);

  auto* mainLayout = new QVBoxLayout(this);

  // Summary label
  m_summaryLabel = new QLabel(this);
  m_summaryLabel->setWordWrap(true);
  QFont summaryFont = m_summaryLabel->font();
  summaryFont.setPointSize(summaryFont.pointSize() + 2);
  summaryFont.setBold(true);
  m_summaryLabel->setFont(summaryFont);
  mainLayout->addWidget(m_summaryLabel);

  // Explanation label
  m_explanationLabel = new QLabel(this);
  m_explanationLabel->setWordWrap(true);
  m_explanationLabel->setStyleSheet("color: #666;");
  mainLayout->addWidget(m_explanationLabel);

  mainLayout->addSpacing(10);

  // Threshold slider - in a container so we can hide it for spread mode
  m_thresholdWidget = new QWidget(this);
  auto* thresholdLayout = new QHBoxLayout(m_thresholdWidget);
  thresholdLayout->setContentsMargins(0, 0, 0, 0);
  m_thresholdLabel = new QLabel(tr("Sensitivity: 30%"), this);
  thresholdLayout->addWidget(m_thresholdLabel);

  m_thresholdSlider = new QSlider(Qt::Horizontal, this);
  m_thresholdSlider->setRange(10, 100);  // 10% to 100% deviation
  m_thresholdSlider->setValue(30);
  m_thresholdSlider->setTickPosition(QSlider::TicksBelow);
  m_thresholdSlider->setTickInterval(10);
  connect(m_thresholdSlider, &QSlider::valueChanged,
          this, &PageSizeWarningDialog::onThresholdChanged);
  thresholdLayout->addWidget(m_thresholdSlider);

  mainLayout->addWidget(m_thresholdWidget);

  // List label
  m_listLabel = new QLabel(tr("Outlier pages (sizes very different from typical):"), this);
  mainLayout->addWidget(m_listLabel);

  // Page list
  m_pagesList = new QListWidget(this);
  m_pagesList->setSelectionMode(QAbstractItemView::ExtendedSelection);
  connect(m_pagesList, &QListWidget::itemDoubleClicked,
          this, &PageSizeWarningDialog::onItemDoubleClicked);
  mainLayout->addWidget(m_pagesList);

  // Button layout - first row
  auto* buttonLayout1 = new QHBoxLayout();

  m_jumpButton = new QPushButton(tr("Jump to Selected"), this);
  m_jumpButton->setEnabled(false);
  connect(m_jumpButton, &QPushButton::clicked,
          this, &PageSizeWarningDialog::onJumpButtonClicked);
  connect(m_pagesList, &QListWidget::itemSelectionChanged, [this]() {
    bool hasSelection = !m_pagesList->selectedItems().isEmpty();
    m_jumpButton->setEnabled(hasSelection);
    m_detachButton->setEnabled(hasSelection);
  });
  buttonLayout1->addWidget(m_jumpButton);

  m_detachButton = new QPushButton(tr("Detach Selected"), this);
  m_detachButton->setEnabled(false);
  m_detachButton->setToolTip(tr("Detach selected pages from aggregate sizing.\n"
                                 "These pages will keep their own size and won't affect other pages."));
  connect(m_detachButton, &QPushButton::clicked,
          this, &PageSizeWarningDialog::onDetachButtonClicked);
  buttonLayout1->addWidget(m_detachButton);

  buttonLayout1->addStretch();

  mainLayout->addLayout(buttonLayout1);

  // Button layout - second row
  auto* buttonLayout2 = new QHBoxLayout();

  m_detachAllButton = new QPushButton(tr("Detach All Listed"), this);
  m_detachAllButton->setToolTip(tr("Detach all listed outlier pages from aggregate sizing."));
  connect(m_detachAllButton, &QPushButton::clicked,
          this, &PageSizeWarningDialog::onDetachAllButtonClicked);
  buttonLayout2->addWidget(m_detachAllButton);

  m_goToSplitButton = new QPushButton(tr("Go to Page Split"), this);
  m_goToSplitButton->setToolTip(tr("Navigate to Page Split (Stage 2) to fix two-page spreads."));
  connect(m_goToSplitButton, &QPushButton::clicked, [this]() {
    emit goToPageSplitStage();
    accept();
  });
  m_goToSplitButton->setVisible(false);  // Only shown in spread mode
  buttonLayout2->addWidget(m_goToSplitButton);

  buttonLayout2->addStretch();

  m_closeButton = new QPushButton(tr("Close"), this);
  connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
  buttonLayout2->addWidget(m_closeButton);

  mainLayout->addLayout(buttonLayout2);
}

void PageSizeWarningDialog::setOutlierPages(int totalPages,
                                             double medianWidthMM,
                                             double medianHeightMM,
                                             double aggregateWidthMM,
                                             double aggregateHeightMM,
                                             const std::vector<OutlierInfo>& outlierPages,
                                             double initialThreshold) {
  m_isSpreadMode = false;
  m_totalPages = totalPages;
  m_medianWidthMM = medianWidthMM;
  m_medianHeightMM = medianHeightMM;
  m_aggregateWidthMM = aggregateWidthMM;
  m_aggregateHeightMM = aggregateHeightMM;
  m_allOutliers = outlierPages;
  m_threshold = initialThreshold;
  m_thresholdSlider->setValue(static_cast<int>((m_threshold - 1.0) * 100));

  // Show outlier mode controls, hide spread mode controls
  m_thresholdWidget->setVisible(true);
  m_goToSplitButton->setVisible(false);

  updateListForThreshold();

  QString summaryText = tr("Outlier pages detected");
  m_summaryLabel->setText(summaryText);

  qDebug() << "PageSizeWarningDialog: received" << m_allOutliers.size() << "outliers"
           << "median:" << medianWidthMM << "x" << medianHeightMM
           << "aggregate:" << aggregateWidthMM << "x" << aggregateHeightMM;

  // Count how many are larger vs smaller
  int largerCount = 0, smallerCount = 0;
  for (const auto& outlier : m_allOutliers) {
    qDebug() << "  Outlier page" << outlier.pageNumber << ":" << outlier.hardWidthMM << "x" << outlier.hardHeightMM
             << "deviation:" << outlier.deviationRatio << "isLarger:" << outlier.isLarger;
    if (outlier.isLarger) largerCount++;
    else smallerCount++;
  }
  qDebug() << "  largerCount:" << largerCount << "smallerCount:" << smallerCount;

  // Check if pages might be unsplit two-page spreads
  // If aggregate width is about 2x the median, it's likely spreads
  double widthRatio = (medianWidthMM > 0) ? (aggregateWidthMM / medianWidthMM) : 1.0;
  bool likelySpreads = (widthRatio > 1.8 && widthRatio < 2.2);

  QString explanation;
  if (likelySpreads && m_allOutliers.empty()) {
    // Pages are likely two-page spreads that need splitting
    explanation = tr(
        "Pages appear to be two-page spreads that need splitting.\n\n"
        "Typical page size: %1\n"
        "Current aggregate: %2\n\n"
        "The aggregate width is about 2x the typical width, which suggests "
        "these are book spreads (facing pages) that weren't detected by the "
        "page splitter.\n\n"
        "Solution: Go back to Page Split (Stage 2) and manually set pages "
        "to 'Two Pages' layout, or adjust the split detection settings.")
        .arg(formatSizeMM(medianWidthMM, medianHeightMM))
        .arg(formatSizeMM(aggregateWidthMM, aggregateHeightMM));
    m_summaryLabel->setText(tr("Two-page spreads detected"));
  } else if (m_allOutliers.empty()) {
    explanation = tr(
        "No outlier pages found at the current threshold.\n\n"
        "Typical page size: %1\n"
        "Current aggregate: %2\n\n"
        "Try lowering the deviation threshold to detect smaller differences.")
        .arg(formatSizeMM(medianWidthMM, medianHeightMM))
        .arg(formatSizeMM(aggregateWidthMM, aggregateHeightMM));
  } else if (largerCount > 0 && smallerCount == 0) {
    explanation = tr(
        "Found %1 page(s) significantly larger than typical pages.\n\n"
        "Typical page size: %2\n"
        "Current aggregate: %3\n\n"
        "Large outlier pages are setting the aggregate size, causing all other pages "
        "to have excessive margins. Detaching these pages will use the typical page "
        "size as the aggregate instead.")
        .arg(largerCount)
        .arg(formatSizeMM(medianWidthMM, medianHeightMM))
        .arg(formatSizeMM(aggregateWidthMM, aggregateHeightMM));
  } else if (smallerCount > 0 && largerCount == 0) {
    explanation = tr(
        "Found %1 page(s) significantly smaller than typical pages.\n\n"
        "Typical page size: %2\n"
        "These smaller pages will have large margins to match the aggregate size.")
        .arg(smallerCount)
        .arg(formatSizeMM(medianWidthMM, medianHeightMM));
  } else {
    explanation = tr(
        "Found pages with significantly different sizes.\n\n"
        "Typical page size: %1\n"
        "Current aggregate: %2\n\n"
        "Larger pages (%3) are setting the aggregate size. "
        "Smaller pages (%4) will have large margins.")
        .arg(formatSizeMM(medianWidthMM, medianHeightMM))
        .arg(formatSizeMM(aggregateWidthMM, aggregateHeightMM))
        .arg(largerCount)
        .arg(smallerCount);
  }
  m_explanationLabel->setText(explanation);
}

void PageSizeWarningDialog::onThresholdChanged(int value) {
  m_threshold = 1.0 + value / 100.0;
  m_thresholdLabel->setText(tr("Sensitivity: %1%").arg(value));
  updateListForThreshold();
}

void PageSizeWarningDialog::updateListForThreshold() {
  m_pagesList->clear();
  m_filteredOutliers.clear();

  for (const auto& outlier : m_allOutliers) {
    // Check if deviation exceeds threshold
    double absDeviation = outlier.isLarger ? outlier.deviationRatio : (1.0 / outlier.deviationRatio);
    if (absDeviation >= m_threshold) {
      m_filteredOutliers.push_back(outlier);
    }
  }

  for (const auto& outlier : m_filteredOutliers) {
    QString sizeIndicator;
    if (outlier.setsAggregateWidth && outlier.setsAggregateHeight) {
      sizeIndicator = tr(" [SETS AGGREGATE SIZE]");
    } else if (outlier.setsAggregateWidth) {
      sizeIndicator = tr(" [sets width]");
    } else if (outlier.setsAggregateHeight) {
      sizeIndicator = tr(" [sets height]");
    }

    QString itemText = tr("Page %1: %2 (%3)%4")
                           .arg(outlier.pageNumber)
                           .arg(formatSizeMM(outlier.hardWidthMM, outlier.hardHeightMM))
                           .arg(formatDeviation(outlier.deviationRatio))
                           .arg(sizeIndicator);

    QListWidgetItem* item = new QListWidgetItem(itemText);

    // Color code: red for larger (causing problem), blue for smaller
    if (outlier.isLarger) {
      if (outlier.setsAggregateWidth || outlier.setsAggregateHeight) {
        item->setForeground(QColor(180, 0, 0));  // Dark red for pages setting aggregate
      } else {
        item->setForeground(QColor(200, 100, 0));  // Orange for other large pages
      }
    } else {
      item->setForeground(QColor(0, 0, 180));  // Blue for smaller pages
    }

    m_pagesList->addItem(item);
  }

  int thresholdPercent = static_cast<int>((m_threshold - 1.0) * 100);
  m_listLabel->setText(tr("Outlier pages (>%1% deviation, %2 pages):")
                           .arg(thresholdPercent)
                           .arg(m_filteredOutliers.size()));

  bool hasItems = !m_filteredOutliers.empty();
  m_pagesList->setVisible(hasItems);
  m_jumpButton->setVisible(hasItems);
  m_detachButton->setVisible(hasItems);
  m_detachAllButton->setVisible(hasItems);

  if (!hasItems) {
    m_listLabel->setText(tr("No pages exceed the %1% deviation threshold.")
                             .arg(thresholdPercent));
  }
}

QString PageSizeWarningDialog::formatSizeMM(double widthMM, double heightMM) const {
  return tr("%1 x %2 mm").arg(widthMM, 0, 'f', 1).arg(heightMM, 0, 'f', 1);
}

QString PageSizeWarningDialog::formatDeviation(double ratio) const {
  if (ratio >= 1.0) {
    int percent = static_cast<int>((ratio - 1.0) * 100);
    return tr("%1% larger").arg(percent);
  } else {
    int percent = static_cast<int>((1.0 / ratio - 1.0) * 100);
    return tr("%1% smaller").arg(percent);
  }
}

void PageSizeWarningDialog::onItemDoubleClicked(QListWidgetItem* item) {
  int row = m_pagesList->row(item);
  // Use the appropriate list based on mode
  if (m_isSpreadMode) {
    if (row >= 0 && row < static_cast<int>(m_spreadPages.size())) {
      emit jumpToPage(m_spreadPages[row].pageId);
    }
  } else {
    if (row >= 0 && row < static_cast<int>(m_filteredOutliers.size())) {
      emit jumpToPage(m_filteredOutliers[row].pageId);
    }
  }
}

void PageSizeWarningDialog::onJumpButtonClicked() {
  int row = m_pagesList->currentRow();
  // Use the appropriate list based on mode
  if (m_isSpreadMode) {
    if (row >= 0 && row < static_cast<int>(m_spreadPages.size())) {
      emit jumpToPage(m_spreadPages[row].pageId);
    }
  } else {
    if (row >= 0 && row < static_cast<int>(m_filteredOutliers.size())) {
      emit jumpToPage(m_filteredOutliers[row].pageId);
    }
  }
}

void PageSizeWarningDialog::onDetachButtonClicked() {
  QList<QListWidgetItem*> selectedItems = m_pagesList->selectedItems();
  if (selectedItems.isEmpty()) {
    return;
  }

  std::vector<PageId> selectedPageIds;
  for (QListWidgetItem* item : selectedItems) {
    int row = m_pagesList->row(item);
    if (row >= 0 && row < static_cast<int>(m_filteredOutliers.size())) {
      selectedPageIds.push_back(m_filteredOutliers[row].pageId);
      QFont font = item->font();
      font.setStrikeOut(true);
      item->setFont(font);
      item->setForeground(Qt::darkGreen);
    }
  }

  if (!selectedPageIds.empty()) {
    emit detachPagesFromSizing(selectedPageIds);
  }
}

void PageSizeWarningDialog::onDetachAllButtonClicked() {
  if (m_filteredOutliers.empty()) {
    return;
  }

  std::vector<PageId> allPageIds;
  for (size_t i = 0; i < m_filteredOutliers.size(); ++i) {
    allPageIds.push_back(m_filteredOutliers[i].pageId);
    QListWidgetItem* item = m_pagesList->item(static_cast<int>(i));
    if (item) {
      QFont font = item->font();
      font.setStrikeOut(true);
      item->setFont(font);
      item->setForeground(Qt::darkGreen);
    }
  }

  emit detachPagesFromSizing(allPageIds);
}

void PageSizeWarningDialog::setSpreadPages(int totalPages,
                                            double medianWidthMM,
                                            double medianHeightMM,
                                            double aggregateWidthMM,
                                            double aggregateHeightMM,
                                            const std::vector<OutlierInfo>& spreadPages) {
  m_isSpreadMode = true;
  m_totalPages = totalPages;
  m_medianWidthMM = medianWidthMM;
  m_medianHeightMM = medianHeightMM;
  m_aggregateWidthMM = aggregateWidthMM;
  m_aggregateHeightMM = aggregateHeightMM;
  m_spreadPages = spreadPages;

  // Hide outlier mode controls, show spread mode controls
  m_thresholdWidget->setVisible(false);
  m_goToSplitButton->setVisible(true);
  m_detachButton->setVisible(false);
  m_detachAllButton->setVisible(false);

  setWindowTitle(tr("Two-Page Spreads Detected"));
  m_summaryLabel->setText(tr("Pages appear to be two-page spreads"));

  QString explanation = tr(
      "The page dimensions suggest these are facing-page spreads "
      "(two book pages scanned together) that need to be split.\n\n"
      "Expected single page: %1\n"
      "Current page size: %2\n\n"
      "The current width is about 2x what a single page should be. "
      "Use Page Split (Stage 2) to split these into individual pages.")
      .arg(formatSizeMM(medianWidthMM, medianHeightMM))
      .arg(formatSizeMM(aggregateWidthMM, aggregateHeightMM));
  m_explanationLabel->setText(explanation);

  // Show all pages as spreads in the list
  m_pagesList->clear();
  m_listLabel->setText(tr("Pages to split (%1 pages):").arg(spreadPages.size()));

  for (const auto& page : spreadPages) {
    QString itemText = tr("Page %1: %2 (spread)")
                           .arg(page.pageNumber)
                           .arg(formatSizeMM(page.hardWidthMM, page.hardHeightMM));
    QListWidgetItem* item = new QListWidgetItem(itemText);
    item->setForeground(QColor(180, 100, 0));  // Orange for spreads
    m_pagesList->addItem(item);
  }

  m_pagesList->setVisible(!spreadPages.empty());
  m_jumpButton->setVisible(!spreadPages.empty());

  qDebug() << "PageSizeWarningDialog: spread mode with" << spreadPages.size() << "pages"
           << "median:" << medianWidthMM << "x" << medianHeightMM
           << "aggregate:" << aggregateWidthMM << "x" << aggregateHeightMM;
}
