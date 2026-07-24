// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_APP_PAGEBOXSUMMARYDIALOG_H_
#define SCANTAILOR_APP_PAGEBOXSUMMARYDIALOG_H_

#include <QDialog>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QButtonGroup>
#include <QSlider>
#include <vector>

#include "PageId.h"

class PageBoxSummaryDialog : public QDialog {
  Q_OBJECT

 public:
  struct PageSummary {
    PageId pageId;
    QString fileName;
    int pageNumber;
    double pageWidth;        // in pixels
    double deviationPercent; // deviation from median width as percentage
  };

  enum ViewMode { VIEW_OUTLIER_PAGES, VIEW_NORMAL_PAGES };

  explicit PageBoxSummaryDialog(QWidget* parent = nullptr);

  void setSummary(int totalPages,
                  const std::vector<PageSummary>& allPages,
                  int initialThresholdPercent = 10);

 signals:
  void jumpToPage(const PageId& pageId);
  void disablePageBoxSelected(const std::vector<PageId>& pageIds);
  void disablePageBoxAll(const std::vector<PageId>& pageIds);

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
  double computeMedianWidth() const;

  QLabel* m_summaryLabel;
  QLabel* m_listLabel;
  QLabel* m_thresholdLabel;
  QSlider* m_thresholdSlider;
  QPushButton* m_viewOutlierButton;
  QPushButton* m_viewNormalButton;
  QButtonGroup* m_viewButtonGroup;
  QListWidget* m_pagesList;
  QPushButton* m_jumpButton;
  QPushButton* m_actionButton;       // "Disable Page Box (Selected)"
  QPushButton* m_actionAllButton;    // "Disable Page Box (All Listed)"
  QPushButton* m_closeButton;

  std::vector<PageSummary> m_allPages;
  std::vector<PageSummary> m_outlierPages;
  std::vector<PageSummary> m_normalPages;
  ViewMode m_currentMode;
  int m_thresholdPercent;
  double m_medianWidth;
};

#endif  // SCANTAILOR_APP_PAGEBOXSUMMARYDIALOG_H_
