// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_APP_BATCHPROCESSINGSUMMARYDIALOG_H_
#define SCANTAILOR_APP_BATCHPROCESSINGSUMMARYDIALOG_H_

#include <QDialog>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <vector>

#include "ImageId.h"

class BatchProcessingSummaryDialog : public QDialog {
  Q_OBJECT

 public:
  struct PageSummary {
    ImageId imageId;
    QString fileName;
    bool isSplit;
  };

  explicit BatchProcessingSummaryDialog(QWidget* parent = nullptr);

  void setSummary(int totalPages, int splitPages, int singlePages,
                  const std::vector<PageSummary>& singlePageList);

 signals:
  void jumpToPage(const ImageId& imageId);
  void forceTwoPageSelected(const std::vector<ImageId>& imageIds);
  void forceTwoPageAll(const std::vector<ImageId>& imageIds);

 private slots:
  void onItemDoubleClicked(QListWidgetItem* item);
  void onJumpButtonClicked();
  void onForceTwoPageClicked();
  void onForceTwoPageAllClicked();

 private:
  QLabel* m_summaryLabel;
  QListWidget* m_singlePagesList;
  QPushButton* m_jumpButton;
  QPushButton* m_forceTwoPageButton;
  QPushButton* m_forceTwoPageAllButton;
  QPushButton* m_closeButton;
  std::vector<PageSummary> m_singlePages;
};

#endif  // SCANTAILOR_APP_BATCHPROCESSINGSUMMARYDIALOG_H_
