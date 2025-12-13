// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_PAGE_LAYOUT_SETTINGS_H_
#define SCANTAILOR_PAGE_LAYOUT_SETTINGS_H_

#include <DeviationProvider.h>

#include <memory>
#include <vector>

#include "Margins.h"
#include "NonCopyable.h"
#include "PageId.h"
class Margins;
class PageSequence;
class AbstractRelinker;
class QSizeF;
class QRectF;

namespace page_layout {
class Params;
class Alignment;
class Guide;

class Settings {
  DECLARE_NON_COPYABLE(Settings)

 public:
  enum AggregateSizeChanged { AGGREGATE_SIZE_UNCHANGED, AGGREGATE_SIZE_CHANGED };

  struct PageSizeInfo {
    PageId pageId;
    double hardWidthMM;
    double hardHeightMM;
    double softMarginRatio;  // How much of the page would be soft margins (0.0 to 1.0)

    PageSizeInfo() = default;
    PageSizeInfo(const PageSizeInfo&) = default;
    PageSizeInfo(PageSizeInfo&&) = default;
    PageSizeInfo& operator=(const PageSizeInfo&) = default;
    PageSizeInfo& operator=(PageSizeInfo&&) = default;
  };

  struct OutlierPageInfo {
    PageId pageId;
    double hardWidthMM;
    double hardHeightMM;
    double medianWidthMM;     // The median page width for comparison
    double medianHeightMM;    // The median page height for comparison
    double deviationRatio;    // How much larger/smaller than median (>1.0 = larger, <1.0 = smaller)
    bool isLarger;            // True if this page is larger than median (causing aggregate size increase)
    bool setsAggregateWidth;  // True if this page sets the aggregate width
    bool setsAggregateHeight; // True if this page sets the aggregate height

    OutlierPageInfo() = default;
    OutlierPageInfo(const OutlierPageInfo&) = default;
    OutlierPageInfo(OutlierPageInfo&&) = default;
    OutlierPageInfo& operator=(const OutlierPageInfo&) = default;
    OutlierPageInfo& operator=(OutlierPageInfo&&) = default;
  };

  Settings();

  virtual ~Settings();

  /**
   * \brief Removes all stored data.
   */
  void clear();

  void performRelinking(const AbstractRelinker& relinker);

  /**
   * \brief Removes all stored data for pages that are not in the provided list.
   */
  void removePagesMissingFrom(const PageSequence& pages);

  /**
   * \brief Check that we have all the essential parameters for every
   *        page in the list.
   *
   * This check is used to allow of forbid going to the output stage.
   * \param pages The list of pages to check.
   * \param ignore The page to be ignored by the check.  Optional.
   */
  bool checkEverythingDefined(const PageSequence& pages, const PageId* ignore = nullptr) const;

  /**
   * \brief Get all page parameters at once.
   *
   * May return a null unique_ptr if the specified page is unknown to us.
   */
  std::unique_ptr<Params> getPageParams(const PageId& pageId) const;

  bool isParamsNull(const PageId& pageId) const;

  /**
   * \brief Set all page parameters at once.
   */
  void setPageParams(const PageId& pageId, const Params& params);

  /**
   * \brief Updates content size and returns all parameters at once.
   */
  Params updateContentSizeAndGetParams(const PageId& pageId,
                                       const QRectF& pageRect,
                                       const QRectF& contentRect,
                                       const QSizeF& contentSizeMm,
                                       QSizeF* aggHardSizeBefore = nullptr,
                                       QSizeF* aggHardSizeAfter = nullptr);

  /**
   * \brief Returns the hard margins for the specified page.
   *
   * Hard margins are margins that will be there no matter what.
   * Soft margins are those added to extend the page to match its
   * size with other pages.
   * \par
   * If no margins were assigned to the specified page, the default
   * margins are returned.
   */
  Margins getHardMarginsMM(const PageId& pageId) const;

  /**
   * \brief Sets hard margins for the specified page.
   *
   * Hard margins are margins that will be there no matter what.
   * Soft margins are those added to extend the page to match its
   * size with other pages.
   */
  void setHardMarginsMM(const PageId& pageId, const Margins& marginsMm);

  /**
   * \brief Returns the alignment for the specified page.
   *
   * Alignments affect the distribution of soft margins.
   * \par
   * If no alignment was specified, the default alignment is returned,
   * which is "center vertically and horizontally".
   */
  Alignment getPageAlignment(const PageId& pageId) const;

  /**
   * \brief Sets alignment for the specified page.
   *
   * Alignments affect the distribution of soft margins and whether this
   * page's size affects others and vice versa.
   */
  AggregateSizeChanged setPageAlignment(const PageId& pageId, const Alignment& alignment);

  /**
   * \brief Disables alignment for multiple pages.
   *
   * This sets the alignment to null for each specified page, meaning they
   * will not be aligned with other pages and won't contribute to the
   * aggregate size calculation.
   * \param pageIds The list of pages to disable alignment for.
   */
  void disableAlignmentForPages(const std::vector<PageId>& pageIds);

  /**
   * \brief Sets content size in millimeters for the specified page.
   *
   * The content size comes from the "Select Content" filter.
   */
  AggregateSizeChanged setContentSizeMM(const PageId& pageId, const QSizeF& contentSizeMm);

  void invalidateContentSize(const PageId& pageId);

  /**
   * \brief Returns the aggregate (max width + max height) hard page size.
   */
  QSizeF getAggregateHardSizeMM() const;

  /**
   * \brief Same as getAggregateHardSizeMM(), but assumes a specified
   *        size and alignment for a specified page.
   *
   * This function doesn't modify anything, it just pretends that
   * the size and alignment of a specified page have changed.
   */
  QSizeF getAggregateHardSizeMM(const PageId& pageId, const QSizeF& hardSizeMm, const Alignment& alignment) const;

  /**
   * \brief Returns pages where soft margins would exceed the given threshold.
   *
   * Used to detect pages with very different sizes that cause excessive soft margins.
   * \param threshold Soft margin ratio threshold (0.0 to 1.0). Pages with soft margin
   *                  ratio above this are returned. Default 0.3 means >30% soft margins.
   * \return List of pages with their size info, sorted by soft margin ratio descending.
   */
  std::vector<PageSizeInfo> getPagesWithExcessiveSoftMargins(double threshold = 0.3) const;

  /**
   * \brief Returns outlier pages whose size differs significantly from the median.
   *
   * Used to identify pages that cause margin issues by being much larger or smaller
   * than most other pages. The user's problematic case is typically when a cover page
   * is larger than all body pages, causing the cover to set the aggregate size.
   *
   * \param deviationThreshold Pages with area ratio > this threshold (or < 1/threshold)
   *                           compared to median are considered outliers. Default 1.3 means
   *                           pages 30% larger or smaller than median.
   * \return List of outlier pages, sorted by deviation (largest outliers first).
   */
  std::vector<OutlierPageInfo> getOutlierPages(double deviationThreshold = 1.3) const;

  /**
   * \brief Returns pages that appear to be unsplit two-page spreads.
   *
   * Identifies pages whose width is approximately 2x the median width,
   * suggesting they are facing-page spreads that weren't split.
   *
   * \return List of spread pages with their size info.
   */
  std::vector<OutlierPageInfo> getUnsplitSpreadPages() const;

  bool isPageAutoMarginsEnabled(const PageId& pageId);

  void setPageAutoMarginsEnabled(const PageId& pageId, bool state);

  const DeviationProvider<PageId>& deviationProvider() const;

  std::vector<Guide>& guides();

  bool isShowingMiddleRectEnabled() const;

  void enableShowingMiddleRect(bool state);

 private:
  class Impl;
  class Item;
  class ModifyMargins;
  class ModifyAlignment;

  class ModifyContentSize;

  std::unique_ptr<Impl> m_impl;
};
}  // namespace page_layout
#endif  // ifndef SCANTAILOR_PAGE_LAYOUT_SETTINGS_H_
