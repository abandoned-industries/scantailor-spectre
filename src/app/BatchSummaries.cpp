// Copyright (C) 2026  ScanTailor Spectre contributors.
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "BatchSummaries.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <set>

#include "AutoManualMode.h"
#include "PageSequence.h"
#include "ProjectPages.h"
#include "StageSequence.h"
#include "ThumbnailSequence.h"
#include "filters/page_box/Filter.h"
#include "filters/page_box/Settings.h"
#include "filters/page_layout/Filter.h"
#include "filters/page_layout/Settings.h"
#include "filters/page_split/Filter.h"
#include "filters/page_split/PageLayout.h"
#include "filters/page_split/Params.h"
#include "filters/page_split/Settings.h"
#include "filters/select_content/Filter.h"
#include "filters/select_content/Params.h"
#include "filters/select_content/Settings.h"

namespace {

using Action = PagesToCheckView::Action;
using ActionId = PagesToCheckView::ActionId;
std::vector<Action> splitActions() {
  return {{ActionId::JumpToPage, QObject::tr("Jump to page")},
          {ActionId::ForceTwoPageLayout, QObject::tr("Force two-page layout")},
          {ActionId::ForceSinglePageLayout, QObject::tr("Force single page layout")},
          {ActionId::ResetAutoLayout, QObject::tr("Reset to auto layout")}};
}

std::vector<Action> pageBoxActions() {
  return {{ActionId::JumpToPage, QObject::tr("Jump to page")},
          {ActionId::DisablePageBox, QObject::tr("Disable page box")}};
}

std::vector<Action> contentActions() {
  return {{ActionId::JumpToPage, QObject::tr("Jump to page")},
          {ActionId::PreserveLayout, QObject::tr("Preserve layout")}};
}

std::vector<Action> marginActions() {
  return {{ActionId::JumpToPage, QObject::tr("Jump to page")},
          {ActionId::DetachFromAggregateSize, QObject::tr("Detach from aggregate size")},
          {ActionId::GoToSplitPages, QObject::tr("Go to Split Pages")}};
}

}  // namespace

BatchSummaries::BatchSummaries(BatchSummariesContext& context) : m_context(context) {}

void BatchSummaries::showBatchProcessingSummary(const QString&, const QString&) {
  ReviewSetup notSplit = pageSplitReview(false);
  ReviewSetup split = pageSplitReview(true);
  if (notSplit.model.totalPages == 0) {
    return;
  }
  m_context.showPagesToCheck(notSplit.firstCount > 0 ? notSplit : split);
}

ReviewSetup BatchSummaries::pageSplitReview(const bool showSplitPages) const {
  ReviewSetup setup;
  setup.stage = ReviewStage::PageSplit;
  setup.showSecondGroup = showSplitPages;
  setup.model.visibleGroup = showSplitPages ? 1 : 0;
  if (!m_context.stages() || !m_context.pages()) {
    return setup;
  }
  auto settings = m_context.stages()->pageSplitFilter()->settings();
  if (!settings) {
    return setup;
  }

  const PageSequence imagePages = m_context.pages()->toPageSequence(IMAGE_VIEW);
  std::set<ImageId> seenImages;
  int imageNumber = 0;
  for (const PageInfo& pageInfo : imagePages) {
    const ImageId& imageId = pageInfo.id().imageId();
    if (!seenImages.insert(imageId).second) {
      continue;
    }
    ++imageNumber;
    const page_split::Settings::Record record = settings->getPageRecord(imageId);
    const page_split::Params* params = record.params();
    const bool isSplit = params && params->pageLayout().type() == page_split::PageLayout::TWO_PAGES;
    if (isSplit) {
      ++setup.secondCount;
    } else {
      ++setup.firstCount;
    }
    setup.model.pages.push_back(
        {pageInfo.id(), imageNumber, isSplit ? tr("split") : tr("not split"), splitActions(), isSplit ? 1 : 0});
  }
  setup.model.totalPages = imageNumber;
  return setup;
}

void BatchSummaries::showPageBoxSummary() {
  const ReviewSetup setup = pageBoxReview(10);
  if (!setup.model.pages.empty()) {
    m_context.showPagesToCheck(setup);
  }
}

ReviewSetup BatchSummaries::pageBoxReview(const int thresholdPercent) const {
  ReviewSetup setup;
  setup.stage = ReviewStage::PageBox;
  setup.threshold = thresholdPercent;
  if (!m_context.stages() || !m_context.pages()) {
    return setup;
  }
  auto settings = m_context.stages()->pageBoxFilter()->settings();
  if (!settings) {
    return setup;
  }

  struct Candidate {
    PageId pageId;
    int pageNumber;
    double width;
  };
  std::vector<Candidate> candidates;
  std::vector<double> widths;
  const PageSequence pages = m_context.pages()->toPageSequence(m_context.currentView());
  int pageNumber = 0;
  for (const PageInfo& pageInfo : pages) {
    ++pageNumber;
    auto params = settings->getPageParams(pageInfo.id());
    if (!params || !params->pageRect().isValid()) {
      continue;
    }
    const double width = params->pageRect().width();
    candidates.push_back({pageInfo.id(), pageNumber, width});
    widths.push_back(width);
  }
  setup.model.totalPages = pages.numPages();
  if (widths.empty()) {
    return setup;
  }
  std::sort(widths.begin(), widths.end());
  const size_t count = widths.size();
  const double median = count % 2 == 0 ? (widths[count / 2 - 1] + widths[count / 2]) / 2.0
                                       : widths[count / 2];
  for (const Candidate& candidate : candidates) {
    const double deviation = median > 0.0 ? std::abs(candidate.width - median) / median * 100.0 : 0.0;
    if (deviation > static_cast<double>(thresholdPercent)) {
      setup.model.pages.push_back(
          {candidate.pageId,
           candidate.pageNumber,
           tr("width %1px · +%2%")
               .arg(static_cast<int>(candidate.width))
               .arg(QString::number(deviation, 'f', 1)),
           pageBoxActions()});
    }
  }
  return setup;
}

void BatchSummaries::showContentCoverageSummary() {
  const ReviewSetup setup = contentCoverageReview(50);
  if (!setup.model.pages.empty()) {
    m_context.showPagesToCheck(setup);
  }
}

ReviewSetup BatchSummaries::contentCoverageReview(const int thresholdPercent) const {
  ReviewSetup setup;
  setup.stage = ReviewStage::SelectContent;
  setup.threshold = thresholdPercent;
  if (!m_context.stages() || !m_context.pages()) {
    return setup;
  }
  auto settings = m_context.stages()->selectContentFilter()->settings();
  if (!settings) {
    return setup;
  }

  const PageSequence pages = m_context.pages()->toPageSequence(m_context.currentView());
  setup.model.totalPages = pages.numPages();
  int pageNumber = 0;
  for (const PageInfo& pageInfo : pages) {
    ++pageNumber;
    std::unique_ptr<select_content::Params> params(settings->getPageParams(pageInfo.id()));
    if (!params || !params->contentRect().isValid() || !params->pageRect().isValid()
        || params->contentDetectionMode() == MODE_DISABLED) {
      continue;
    }
    const double pageArea = params->pageRect().width() * params->pageRect().height();
    const double contentArea = params->contentRect().width() * params->contentRect().height();
    const double coverage = pageArea > 0.0 ? contentArea / pageArea : 1.0;
    if (coverage < thresholdPercent / 100.0) {
      setup.model.pages.push_back(
          {pageInfo.id(), pageNumber, tr("%1% coverage").arg(static_cast<int>(coverage * 100)), contentActions()});
    }
  }
  return setup;
}

void BatchSummaries::showPageSizeWarning() {
  refreshMarginCandidates();
  const ReviewSetup setup = pageSizeReview(30);
  if (!setup.model.pages.empty()) {
    m_context.showPagesToCheck(setup);
  }
}

void BatchSummaries::refreshMarginCandidates() {
  m_marginCandidates.clear();
  m_marginSpreadMode = false;
  if (!m_context.stages() || !m_context.pages()) {
    return;
  }
  auto settings = m_context.stages()->pageLayoutFilter()->settings();
  if (!settings) {
    return;
  }
  const QSizeF aggregate = settings->getAggregateHardSizeMM();
  if (!aggregate.isValid() || aggregate.isEmpty()) {
    return;
  }
  auto candidates = settings->getOutlierPages(1.3);
  double medianWidth = 0.0;
  if (!candidates.empty()) {
    medianWidth = candidates.front().medianWidthMM;
  } else if (aggregate.height() > 0.0 && aggregate.width() / aggregate.height() > 1.3) {
    medianWidth = aggregate.width() / 2.0;
  } else {
    return;
  }
  const double widthRatio = medianWidth > 0.0 ? aggregate.width() / medianWidth : 1.0;
  m_marginSpreadMode = widthRatio > 1.8 && widthRatio < 2.2;
  if (m_marginSpreadMode) {
    candidates = settings->getUnsplitSpreadPages();
  }

  const PageSequence numberedPages = m_context.pages()->toPageSequence(
      m_marginSpreadMode ? IMAGE_VIEW : m_context.currentView());
  std::set<ImageId> seenImages;
  for (const auto& candidate : candidates) {
    if (m_marginSpreadMode && !seenImages.insert(candidate.pageId.imageId()).second) {
      continue;
    }
    int pageNumber = 0;
    bool found = false;
    for (const PageInfo& pageInfo : numberedPages) {
      ++pageNumber;
      if (pageInfo.id().imageId() == candidate.pageId.imageId()) {
        found = true;
        break;
      }
    }
    if (found) {
      m_marginCandidates.push_back({candidate.pageId,
                                    pageNumber,
                                    candidate.hardWidthMM,
                                    candidate.hardHeightMM,
                                    candidate.deviationRatio,
                                    candidate.isLarger});
    }
  }
}

ReviewSetup BatchSummaries::pageSizeReview(const int sensitivityPercent) const {
  ReviewSetup setup;
  setup.stage = ReviewStage::Margins;
  setup.threshold = sensitivityPercent;
  setup.model.totalPages = m_context.pages()
                               ? m_context.pages()->toPageSequence(m_context.currentView()).numPages()
                               : 0;
  const double threshold = 1.0 + sensitivityPercent / 100.0;
  for (const MarginCandidate& candidate : m_marginCandidates) {
    const double absoluteDeviation = candidate.isLarger ? candidate.deviationRatio
                                                        : 1.0 / candidate.deviationRatio;
    if (!m_marginSpreadMode && absoluteDeviation < threshold) {
      continue;
    }
    const int deviationPercent = static_cast<int>((absoluteDeviation - 1.0) * 100.0);
    setup.model.pages.push_back(
        {candidate.pageId,
         candidate.pageNumber,
         tr("%1 × %2 mm · +%3%")
             .arg(candidate.hardWidthMM, 0, 'f', 0)
             .arg(candidate.hardHeightMM, 0, 'f', 0)
             .arg(deviationPercent),
         marginActions()});
  }
  return setup;
}

void BatchSummaries::forceTwoPageForImages(const std::vector<ImageId>& imageIds) {
  setPageSplitLayout(imageIds, page_split::TWO_PAGES);
}

void BatchSummaries::forceSinglePageForImages(const std::vector<ImageId>& imageIds) {
  setPageSplitLayout(imageIds, page_split::SINGLE_PAGE_UNCUT);
}

void BatchSummaries::resetAutoLayoutForImages(const std::vector<ImageId>& imageIds) {
  setPageSplitLayout(imageIds, page_split::AUTO_LAYOUT_TYPE);
}

void BatchSummaries::setPageSplitLayout(const std::vector<ImageId>& imageIds, const int layoutType) {
  if (imageIds.empty() || !m_context.stages() || !m_context.pages()) {
    return;
  }
  std::set<PageId> pageIds;
  const PageSequence pages = m_context.pages()->toPageSequence(IMAGE_VIEW);
  for (const PageInfo& pageInfo : pages) {
    if (std::find(imageIds.begin(), imageIds.end(), pageInfo.id().imageId()) != imageIds.end()) {
      pageIds.insert(pageInfo.id());
    }
  }
  if (pageIds.empty()) {
    return;
  }
  m_context.stages()->pageSplitFilter()->settings()->setLayoutTypeFor(
      static_cast<page_split::LayoutType>(layoutType), pageIds);
  if (m_context.thumbSequence()) {
    for (const PageId& pageId : pageIds) {
      m_context.thumbSequence()->invalidateThumbnail(pageId);
    }
  }
  if (!m_context.isReviewMode()
      && m_context.currentFilterIndex() == m_context.stages()->pageSplitFilterIdx()) {
    m_context.updateMainArea();
  }
}

void BatchSummaries::disablePageBoxForPages(const std::vector<PageId>& pageIds) {
  if (pageIds.empty() || !m_context.stages()) {
    return;
  }
  auto settings = m_context.stages()->pageBoxFilter()->settings();
  if (!settings) {
    return;
  }
  for (const PageId& pageId : pageIds) {
    auto params = settings->getPageParams(pageId);
    if (params) {
      params->setPageDetectionMode(MODE_DISABLED);
      settings->setPageParams(pageId, *params);
    }
  }
  if (m_context.thumbSequence()) {
    m_context.thumbSequence()->invalidateAllThumbnails();
  }
}

void BatchSummaries::preserveLayoutForPages(const std::vector<PageId>& pageIds) {
  if (pageIds.empty() || !m_context.stages()) {
    return;
  }
  auto settings = m_context.stages()->selectContentFilter()->settings();
  if (!settings) {
    return;
  }
  for (const PageId& pageId : pageIds) {
    std::unique_ptr<select_content::Params> params(settings->getPageParams(pageId));
    if (!params) {
      continue;
    }
    params->setContentDetectionMode(MODE_DISABLED);
    params->setContentRect(params->pageRect());
    settings->setPageParams(pageId, *params);
    if (m_context.thumbSequence()) {
      m_context.thumbSequence()->invalidateThumbnail(pageId);
    }
  }
  if (!m_context.isReviewMode()
      && m_context.currentFilterIndex() == m_context.stages()->selectContentFilterIdx()) {
    m_context.updateMainArea();
  }
}

void BatchSummaries::disableAlignmentForPages(const std::vector<PageId>& pageIds) {
  if (pageIds.empty() || !m_context.stages()) {
    return;
  }
  auto settings = m_context.stages()->pageLayoutFilter()->settings();
  if (!settings) {
    return;
  }
  settings->disableAlignmentForPages(pageIds);
  if (m_context.thumbSequence()) {
    m_context.thumbSequence()->invalidateAllThumbnails();
  }
  if (!m_context.isReviewMode()
      && m_context.currentFilterIndex() == m_context.stages()->pageLayoutFilterIdx()) {
    m_context.updateMainArea();
  }
}
