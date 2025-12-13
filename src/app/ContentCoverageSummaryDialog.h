// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_APP_CONTENTCOVERAGESUMMARYDIALOG_H_
#define SCANTAILOR_APP_CONTENTCOVERAGESUMMARYDIALOG_H_

#include <QDialog>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QButtonGroup>
#include <QSlider>
#include <vector>

#include "PageId.h"

class ContentCoverageSummaryDialog : public QDialog {
  Q_OBJECT

 public:
  struct PageSummary {
    PageId pageId;
    QString fileName;
    int pageNumber;
    double coverageRatio;  // contentArea / pageArea (0.0 to 1.0)
  };

  enum ViewMode { VIEW_LOW_COVERAGE, VIEW_NORMAL_COVERAGE };

  explicit ContentCoverageSummaryDialog(QWidget* parent = nullptr);

  void setSummary(int totalPages,
                  const std::vector<PageSummary>& allPages,
                  double initialThreshold = 0.5);

 signals:
  void jumpToPage(const PageId& pageId);
  void preserveLayoutSelected(const std::vector<PageId>& pageIds);
  void preserveLayoutAll(const std::vector<PageId>& pageIds);

 private slots:
  void onItemDoubleClicked(QListWidgetItem* item);
  void onJumpButtonClicked();
  void onActionButtonClicked();
  void onActionAllButtonClicked();
  void onViewModeChanged(int mode);
  void onThresholdChanged(int value);

 private:
  void updateListsForThreshold();
  void updateListForCurrentMode();
  void updateButtonsForCurrentMode();
  const std::vector<PageSummary>& currentPageList() const;

  QLabel* m_summaryLabel;
  QLabel* m_listLabel;
  QLabel* m_thresholdLabel;
  QSlider* m_thresholdSlider;
  QPushButton* m_viewLowCoverageButton;
  QPushButton* m_viewNormalCoverageButton;
  QButtonGroup* m_viewButtonGroup;
  QListWidget* m_pagesList;
  QPushButton* m_jumpButton;
  QPushButton* m_actionButton;       // "Preserve Layout"
  QPushButton* m_actionAllButton;    // "Preserve Layout (All)"
  QPushButton* m_closeButton;

  std::vector<PageSummary> m_allPages;
  std::vector<PageSummary> m_lowCoveragePages;
  std::vector<PageSummary> m_normalCoveragePages;
  ViewMode m_currentMode;
  double m_threshold;
};

#endif  // SCANTAILOR_APP_CONTENTCOVERAGESUMMARYDIALOG_H_
