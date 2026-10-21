// Copyright (C) 2026  ScanTailor Spectre contributors.
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_APP_BATCHSUMMARIES_H_
#define SCANTAILOR_APP_BATCHSUMMARIES_H_

#include <QObject>
#include <vector>

#include "PageView.h"
#include "PagesToCheckView.h"

class ImageId;
class PageId;
class ProjectPages;
class StageSequence;
class ThumbnailSequence;

enum class ReviewStage { PageSplit, PageBox, SelectContent, Margins };

struct ReviewSetup {
  ReviewStage stage = ReviewStage::PageSplit;
  PagesToCheckView::ReviewModel model;
  int threshold = 0;
  int firstCount = 0;
  int secondCount = 0;
  bool showSecondGroup = false;
};

class BatchSummariesContext {
 public:
  virtual ~BatchSummariesContext() = default;

  virtual StageSequence* stages() const = 0;
  virtual ProjectPages* pages() const = 0;
  virtual ThumbnailSequence* thumbSequence() const = 0;
  virtual PageView currentView() const = 0;
  virtual int currentFilterIndex() const = 0;
  virtual bool isReviewMode() const = 0;
  virtual void jumpToPage(const PageId& pageId) = 0;
  virtual void updateMainArea() = 0;
  virtual void selectFilterListRow(int row) = 0;
  virtual void showPagesToCheck(const ReviewSetup& setup) = 0;
};

class BatchSummaries : public QObject {
  Q_OBJECT
 public:
  explicit BatchSummaries(BatchSummariesContext& context);

  void showBatchProcessingSummary(const QString& timingSummary, const QString& timingBreakdown);
  void showPageBoxSummary();
  void showContentCoverageSummary();
  void showPageSizeWarning();

  ReviewSetup pageSplitReview(bool showSplitPages) const;
  ReviewSetup pageBoxReview(int thresholdPercent) const;
  ReviewSetup contentCoverageReview(int thresholdPercent) const;
  ReviewSetup pageSizeReview(int sensitivityPercent) const;

  void forceTwoPageForImages(const std::vector<ImageId>& imageIds);
  void forceSinglePageForImages(const std::vector<ImageId>& imageIds);
  void resetAutoLayoutForImages(const std::vector<ImageId>& imageIds);
  void disablePageBoxForPages(const std::vector<PageId>& pageIds);
  void preserveLayoutForPages(const std::vector<PageId>& pageIds);
  void disableAlignmentForPages(const std::vector<PageId>& pageIds);

 private:
  struct MarginCandidate {
    PageId pageId;
    int pageNumber = 0;
    double hardWidthMM = 0.0;
    double hardHeightMM = 0.0;
    double deviationRatio = 1.0;
    bool isLarger = false;
  };

  void setPageSplitLayout(const std::vector<ImageId>& imageIds, int layoutType);
  void refreshMarginCandidates();

  BatchSummariesContext& m_context;
  std::vector<MarginCandidate> m_marginCandidates;
  bool m_marginSpreadMode = false;
};

#endif  // SCANTAILOR_APP_BATCHSUMMARIES_H_
