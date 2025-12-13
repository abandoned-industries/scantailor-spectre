// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_APP_PAGESIZEWARNINGDIALOG_H_
#define SCANTAILOR_APP_PAGESIZEWARNINGDIALOG_H_

#include <QDialog>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <vector>

#include "PageId.h"

/**
 * \brief Dialog that shows outlier pages with sizes significantly different from the median.
 *
 * When a document has pages of very different sizes (e.g., a larger cover page among smaller
 * body pages), the largest pages set the "aggregate size" and all other pages get soft margins
 * to fill the space. This dialog helps users identify such outlier pages and detach them from
 * the aggregate size calculation.
 */
class PageSizeWarningDialog : public QDialog {
  Q_OBJECT

 public:
  struct OutlierInfo {
    PageId pageId;
    QString fileName;
    int pageNumber;
    double hardWidthMM;
    double hardHeightMM;
    double medianWidthMM;
    double medianHeightMM;
    double deviationRatio;  // >1.0 = larger than median, <1.0 = smaller
    bool isLarger;
    bool setsAggregateWidth;
    bool setsAggregateHeight;

    OutlierInfo() = default;
    OutlierInfo(const OutlierInfo&) = default;
    OutlierInfo(OutlierInfo&&) = default;
    OutlierInfo& operator=(const OutlierInfo&) = default;
    OutlierInfo& operator=(OutlierInfo&&) = default;
  };

  explicit PageSizeWarningDialog(QWidget* parent = nullptr);

  void setOutlierPages(int totalPages,
                       double medianWidthMM,
                       double medianHeightMM,
                       double aggregateWidthMM,
                       double aggregateHeightMM,
                       const std::vector<OutlierInfo>& outlierPages,
                       double initialThreshold = 1.3);

  void setSpreadPages(int totalPages,
                      double medianWidthMM,
                      double medianHeightMM,
                      double aggregateWidthMM,
                      double aggregateHeightMM,
                      const std::vector<OutlierInfo>& spreadPages);

 signals:
  void jumpToPage(const PageId& pageId);
  void detachPagesFromSizing(const std::vector<PageId>& pageIds);
  void goToPageSplitStage();

 private slots:
  void onItemDoubleClicked(QListWidgetItem* item);
  void onJumpButtonClicked();
  void onDetachButtonClicked();
  void onDetachAllButtonClicked();
  void onThresholdChanged(int value);

 private:
  void updateListForThreshold();
  QString formatSizeMM(double widthMM, double heightMM) const;
  QString formatDeviation(double ratio) const;

  QLabel* m_summaryLabel;
  QLabel* m_explanationLabel;
  QLabel* m_thresholdLabel;
  QSlider* m_thresholdSlider;
  QLabel* m_listLabel;
  QListWidget* m_pagesList;
  QPushButton* m_jumpButton;
  QPushButton* m_detachButton;
  QPushButton* m_detachAllButton;
  QPushButton* m_goToSplitButton;
  QPushButton* m_closeButton;
  QWidget* m_thresholdWidget;  // Container for threshold controls

  std::vector<OutlierInfo> m_allOutliers;
  std::vector<OutlierInfo> m_spreadPages;  // Pages detected as spreads
  bool m_isSpreadMode;
  std::vector<OutlierInfo> m_filteredOutliers;
  double m_medianWidthMM;
  double m_medianHeightMM;
  double m_aggregateWidthMM;
  double m_aggregateHeightMM;
  double m_threshold;
  int m_totalPages;
};

#endif  // SCANTAILOR_APP_PAGESIZEWARNINGDIALOG_H_
