// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_APP_BATCHPROCESSINGSUMMARYDIALOG_H_
#define SCANTAILOR_APP_BATCHPROCESSINGSUMMARYDIALOG_H_

#include <QDialog>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QButtonGroup>
#include <vector>

#include "ImageId.h"

class BatchProcessingSummaryDialog : public QDialog {
  Q_OBJECT

 public:
  struct PageSummary {
    ImageId imageId;
    QString fileName;
    int pageNumber;
    bool isSplit;
  };

  enum ViewMode { VIEW_NOT_SPLIT, VIEW_SPLIT };

  explicit BatchProcessingSummaryDialog(QWidget* parent = nullptr);

  void setSummary(int totalPages, int splitPages, int singlePages,
                  const std::vector<PageSummary>& singlePageList,
                  const std::vector<PageSummary>& splitPageList);

 signals:
  void jumpToPage(const ImageId& imageId);
  void forceTwoPageSelected(const std::vector<ImageId>& imageIds);
  void forceTwoPageAll(const std::vector<ImageId>& imageIds);
  void forceSinglePageSelected(const std::vector<ImageId>& imageIds);
  void forceSinglePageAll(const std::vector<ImageId>& imageIds);

 private slots:
  void onItemDoubleClicked(QListWidgetItem* item);
  void onJumpButtonClicked();
  void onActionButtonClicked();
  void onActionAllButtonClicked();
  void onViewModeChanged(int mode);

 private:
  void updateListForCurrentMode();
  void updateButtonsForCurrentMode();
  const std::vector<PageSummary>& currentPageList() const;

  QLabel* m_summaryLabel;
  QLabel* m_listLabel;
  QPushButton* m_viewNotSplitButton;
  QPushButton* m_viewSplitButton;
  QButtonGroup* m_viewButtonGroup;
  QListWidget* m_pagesList;
  QPushButton* m_jumpButton;
  QPushButton* m_actionButton;       // "Force Two-Page" or "Force Single Page"
  QPushButton* m_actionAllButton;    // "Force Two-Page (All)" or "Force Single Page (All)"
  QPushButton* m_closeButton;

  std::vector<PageSummary> m_singlePages;
  std::vector<PageSummary> m_splitPages;
  ViewMode m_currentMode;
};

#endif  // SCANTAILOR_APP_BATCHPROCESSINGSUMMARYDIALOG_H_
