// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "Settings.h"

#include <DeviationProvider.h>

#include <QDebug>
#include <QMutex>
#include <algorithm>
#include <boost/foreach.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index_container.hpp>
#include <utility>

#include "AbstractRelinker.h"
#include "Guide.h"
#include "PageId.h"
#include "PageSequence.h"
#include "Params.h"
#include "RelinkablePath.h"


using namespace ::boost;
using namespace ::boost::multi_index;

namespace page_layout {
class Settings::Item {
 public:
  PageId pageId;
  Margins hardMarginsMM;
  QRectF pageRect;
  QRectF contentRect;
  QSizeF contentSizeMM;
  Alignment alignment;
  bool autoMargins;

  Item(const PageId& pageId,
       const Margins& hardMarginsMm,
       const QRectF& pageRect,
       const QRectF& contentRect,
       const QSizeF& contentSizeMm,
       const Alignment& alignment,
       bool autoMargins);

  double hardWidthMM() const;

  double hardHeightMM() const;

  double influenceHardWidthMM() const;

  double influenceHardHeightMM() const;

  bool alignedWithOthers() const { return !alignment.isNull(); }
};


class Settings::ModifyMargins {
 public:
  ModifyMargins(const Margins& marginsMm, const bool autoMargins)
      : m_marginsMM(marginsMm), m_autoMargins(autoMargins) {}

  void operator()(Item& item) {
    item.hardMarginsMM = m_marginsMM;
    item.autoMargins = m_autoMargins;
  }

 private:
  Margins m_marginsMM;
  bool m_autoMargins;
};


class Settings::ModifyAlignment {
 public:
  explicit ModifyAlignment(const Alignment& alignment) : m_alignment(alignment) {}

  void operator()(Item& item) { item.alignment = m_alignment; }

 private:
  Alignment m_alignment;
};


class Settings::ModifyContentSize {
 public:
  ModifyContentSize(const QSizeF& contentSizeMm, const QRectF& contentRect, const QRectF& pageRect)
      : m_contentSizeMM(contentSizeMm), m_contentRect(contentRect), m_pageRect(pageRect) {}

  void operator()(Item& item) {
    item.contentSizeMM = m_contentSizeMM;
    item.contentRect = m_contentRect;
    item.pageRect = m_pageRect;
  }

 private:
  QSizeF m_contentSizeMM;
  QRectF m_contentRect;
  QRectF m_pageRect;
};


class Settings::Impl {
 public:
  Impl();

  ~Impl();

  void clear();

  void performRelinking(const AbstractRelinker& relinker);

  void removePagesMissingFrom(const PageSequence& pages);

  bool checkEverythingDefined(const PageSequence& pages, const PageId* ignore) const;

  std::unique_ptr<Params> getPageParams(const PageId& pageId) const;

  bool isParamsNull(const PageId& pageId) const;

  void setPageParams(const PageId& pageId, const Params& params);

  Params updateContentSizeAndGetParams(const PageId& pageId,
                                       const QRectF& pageRect,
                                       const QRectF& contentRect,
                                       const QSizeF& contentSizeMm,
                                       QSizeF* aggHardSizeBefore,
                                       QSizeF* aggHardSizeAfter);

  Margins getHardMarginsMM(const PageId& pageId) const;

  void setHardMarginsMM(const PageId& pageId, const Margins& marginsMm);

  Alignment getPageAlignment(const PageId& pageId) const;

  AggregateSizeChanged setPageAlignment(const PageId& pageId, const Alignment& alignment);

  void disableAlignmentForPages(const std::vector<PageId>& pageIds);

  AggregateSizeChanged setContentSizeMM(const PageId& pageId, const QSizeF& contentSizeMm);

  void invalidateContentSize(const PageId& pageId);

  QSizeF getAggregateHardSizeMM() const;

  QSizeF getAggregateHardSizeMMLocked() const;

  QSizeF getAggregateHardSizeMM(const PageId& pageId, const QSizeF& hardSizeMm, const Alignment& alignment) const;

  std::vector<Settings::PageSizeInfo> getPagesWithExcessiveSoftMargins(double threshold) const;

  std::vector<Settings::OutlierPageInfo> getOutlierPages(double deviationThreshold) const;

  std::vector<Settings::OutlierPageInfo> getUnsplitSpreadPages() const;

  bool isPageAutoMarginsEnabled(const PageId& pageId);

  void setPageAutoMarginsEnabled(const PageId& pageId, bool state);

  const DeviationProvider<PageId>& deviationProvider() const;

  std::vector<Guide>& guides();

  bool isShowingMiddleRectEnabled() const;

  void enableShowingMiddleRect(bool state);

 private:
  class SequencedTag;
  class DescWidthTag;
  class DescHeightTag;

  using Container = multi_index_container<
      Item,
      indexed_by<hashed_unique<member<Item, PageId, &Item::pageId>, std::hash<PageId>>,
                 sequenced<tag<SequencedTag>>,
                 ordered_non_unique<tag<DescWidthTag>,
                                    // ORDER BY alignedWithOthers DESC, hardWidthMM DESC
                                    composite_key<Item,
                                                  const_mem_fun<Item, bool, &Item::alignedWithOthers>,
                                                  const_mem_fun<Item, double, &Item::hardWidthMM>>,
                                    composite_key_compare<std::greater<>, std::greater<double>>>,
                 ordered_non_unique<tag<DescHeightTag>,
                                    // ORDER BY alignedWithOthers DESC, hardHeightMM DESC
                                    composite_key<Item,
                                                  const_mem_fun<Item, bool, &Item::alignedWithOthers>,
                                                  const_mem_fun<Item, double, &Item::hardHeightMM>>,
                                    composite_key_compare<std::greater<>, std::greater<double>>>>>;

  using UnorderedItems = Container::index<SequencedTag>::type;
  using DescWidthOrder = Container::index<DescWidthTag>::type;
  using DescHeightOrder = Container::index<DescHeightTag>::type;

  mutable QMutex m_mutex;
  Container m_items;
  UnorderedItems& m_unorderedItems;
  DescWidthOrder& m_descWidthOrder;
  DescHeightOrder& m_descHeightOrder;
  const QRectF m_invalidRect;
  const QSizeF m_invalidSize;
  const Margins m_defaultHardMarginsMM;
  const Alignment m_defaultAlignment;
  const bool m_autoMarginsDefault;
  DeviationProvider<PageId> m_deviationProvider;
  std::vector<Guide> m_guides;
  bool m_showMiddleRect;
};


/*=============================== Settings ==================================*/

Settings::Settings() : m_impl(std::make_unique<Impl>()) {}

Settings::~Settings() = default;

void Settings::clear() {
  return m_impl->clear();
}

void Settings::performRelinking(const AbstractRelinker& relinker) {
  m_impl->performRelinking(relinker);
}

void Settings::removePagesMissingFrom(const PageSequence& pages) {
  m_impl->removePagesMissingFrom(pages);
}

bool Settings::checkEverythingDefined(const PageSequence& pages, const PageId* ignore) const {
  return m_impl->checkEverythingDefined(pages, ignore);
}

std::unique_ptr<Params> Settings::getPageParams(const PageId& pageId) const {
  return m_impl->getPageParams(pageId);
}

void Settings::setPageParams(const PageId& pageId, const Params& params) {
  return m_impl->setPageParams(pageId, params);
}

Params Settings::updateContentSizeAndGetParams(const PageId& pageId,
                                               const QRectF& pageRect,
                                               const QRectF& contentRect,
                                               const QSizeF& contentSizeMm,
                                               QSizeF* aggHardSizeBefore,
                                               QSizeF* aggHardSizeAfter) {
  return m_impl->updateContentSizeAndGetParams(pageId, pageRect, contentRect, contentSizeMm, aggHardSizeBefore,
                                               aggHardSizeAfter);
}

Margins Settings::getHardMarginsMM(const PageId& pageId) const {
  return m_impl->getHardMarginsMM(pageId);
}

void Settings::setHardMarginsMM(const PageId& pageId, const Margins& marginsMm) {
  m_impl->setHardMarginsMM(pageId, marginsMm);
}

Alignment Settings::getPageAlignment(const PageId& pageId) const {
  return m_impl->getPageAlignment(pageId);
}

Settings::AggregateSizeChanged Settings::setPageAlignment(const PageId& pageId, const Alignment& alignment) {
  return m_impl->setPageAlignment(pageId, alignment);
}

void Settings::disableAlignmentForPages(const std::vector<PageId>& pageIds) {
  m_impl->disableAlignmentForPages(pageIds);
}

Settings::AggregateSizeChanged Settings::setContentSizeMM(const PageId& pageId, const QSizeF& contentSizeMm) {
  return m_impl->setContentSizeMM(pageId, contentSizeMm);
}

void Settings::invalidateContentSize(const PageId& pageId) {
  return m_impl->invalidateContentSize(pageId);
}

QSizeF Settings::getAggregateHardSizeMM() const {
  return m_impl->getAggregateHardSizeMM();
}

QSizeF Settings::getAggregateHardSizeMM(const PageId& pageId,
                                        const QSizeF& hardSizeMm,
                                        const Alignment& alignment) const {
  return m_impl->getAggregateHardSizeMM(pageId, hardSizeMm, alignment);
}

std::vector<Settings::PageSizeInfo> Settings::getPagesWithExcessiveSoftMargins(double threshold) const {
  return m_impl->getPagesWithExcessiveSoftMargins(threshold);
}

std::vector<Settings::OutlierPageInfo> Settings::getOutlierPages(double deviationThreshold) const {
  return m_impl->getOutlierPages(deviationThreshold);
}

std::vector<Settings::OutlierPageInfo> Settings::getUnsplitSpreadPages() const {
  return m_impl->getUnsplitSpreadPages();
}

bool Settings::isPageAutoMarginsEnabled(const PageId& pageId) {
  return m_impl->isPageAutoMarginsEnabled(pageId);
}

void Settings::setPageAutoMarginsEnabled(const PageId& pageId, const bool state) {
  return m_impl->setPageAutoMarginsEnabled(pageId, state);
}

bool Settings::isParamsNull(const PageId& pageId) const {
  return m_impl->isParamsNull(pageId);
}

const DeviationProvider<PageId>& Settings::deviationProvider() const {
  return m_impl->deviationProvider();
}

std::vector<Guide>& Settings::guides() {
  return m_impl->guides();
}

bool Settings::isShowingMiddleRectEnabled() const {
  return m_impl->isShowingMiddleRectEnabled();
}

void Settings::enableShowingMiddleRect(const bool state) {
  m_impl->enableShowingMiddleRect(state);
}

/*============================== Settings::Item =============================*/

Settings::Item::Item(const PageId& pageId,
                     const Margins& hardMarginsMm,
                     const QRectF& pageRect,
                     const QRectF& contentRect,
                     const QSizeF& contentSizeMm,
                     const Alignment& alignment,
                     const bool autoMargins)
    : pageId(pageId),
      hardMarginsMM(hardMarginsMm),
      pageRect(pageRect),
      contentRect(contentRect),
      contentSizeMM(contentSizeMm),
      alignment(alignment),
      autoMargins(autoMargins) {}

double Settings::Item::hardWidthMM() const {
  return contentSizeMM.width() + hardMarginsMM.left() + hardMarginsMM.right();
}

double Settings::Item::hardHeightMM() const {
  return contentSizeMM.height() + hardMarginsMM.top() + hardMarginsMM.bottom();
}

double Settings::Item::influenceHardWidthMM() const {
  return alignment.isNull() ? 0.0 : hardWidthMM();
}

double Settings::Item::influenceHardHeightMM() const {
  return alignment.isNull() ? 0.0 : hardHeightMM();
}

/*============================= Settings::Impl ==============================*/

Settings::Impl::Impl()
    : m_items(),
      m_unorderedItems(m_items.get<SequencedTag>()),
      m_descWidthOrder(m_items.get<DescWidthTag>()),
      m_descHeightOrder(m_items.get<DescHeightTag>()),
      m_invalidRect(),
      m_invalidSize(),
      m_defaultHardMarginsMM(Margins(10.0, 5.0, 10.0, 5.0)),
      m_defaultAlignment(Alignment::TOP, Alignment::HCENTER),
      m_autoMarginsDefault(false),
      m_showMiddleRect(true) {
  m_deviationProvider.setComputeValueByKey([this](const PageId& pageId) -> double {
    auto it = m_items.find(pageId);
    if (it != m_items.end()) {
      const Margins& marginsMm = it->hardMarginsMM;
      double horHardMargins = marginsMm.left() + marginsMm.right();
      double vertHardMargins = marginsMm.top() + marginsMm.bottom();
      return std::sqrt(std::pow(horHardMargins, 2) + std::pow(vertHardMargins, 2));
    }
    return NAN;
  });
}

Settings::Impl::~Impl() = default;

void Settings::Impl::clear() {
  const QMutexLocker locker(&m_mutex);
  m_items.clear();
  m_deviationProvider.clear();
}

void Settings::Impl::performRelinking(const AbstractRelinker& relinker) {
  QMutexLocker locker(&m_mutex);
  Container newItems;

  for (const Item& item : m_unorderedItems) {
    const RelinkablePath oldPath(item.pageId.imageId().filePath(), RelinkablePath::File);
    Item newItem(item);
    newItem.pageId.imageId().setFilePath(relinker.substitutionPathFor(oldPath));
    newItems.insert(newItem);
  }

  m_items.swap(newItems);

  m_deviationProvider.clear();
  for (const Item& item : m_unorderedItems) {
    m_deviationProvider.addOrUpdate(item.pageId);
  }
}

void Settings::Impl::removePagesMissingFrom(const PageSequence& pages) {
  const QMutexLocker locker(&m_mutex);

  std::vector<PageId> sortedPages;

  sortedPages.reserve(pages.numPages());
  for (const PageInfo& page : pages) {
    sortedPages.push_back(page.id());
  }
  std::sort(sortedPages.begin(), sortedPages.end());

  UnorderedItems::const_iterator it(m_unorderedItems.begin());
  const UnorderedItems::const_iterator end(m_unorderedItems.end());
  while (it != end) {
    if (std::binary_search(sortedPages.begin(), sortedPages.end(), it->pageId)) {
      ++it;
    } else {
      m_deviationProvider.remove(it->pageId);
      m_unorderedItems.erase(it++);
    }
  }
}

bool Settings::Impl::checkEverythingDefined(const PageSequence& pages, const PageId* ignore) const {
  const QMutexLocker locker(&m_mutex);

  for (const PageInfo& pageInfo : pages) {
    if (ignore && (*ignore == pageInfo.id())) {
      continue;
    }
    const Container::iterator it(m_items.find(pageInfo.id()));
    if ((it == m_items.end()) || !it->contentSizeMM.isValid()) {
      return false;
    }
  }
  return true;
}

std::unique_ptr<Params> Settings::Impl::getPageParams(const PageId& pageId) const {
  const QMutexLocker locker(&m_mutex);

  const Container::iterator it(m_items.find(pageId));
  if (it == m_items.end()) {
    return nullptr;
  }
  return std::make_unique<Params>(it->hardMarginsMM, it->pageRect, it->contentRect, it->contentSizeMM, it->alignment,
                                  it->autoMargins);
}

void Settings::Impl::setPageParams(const PageId& pageId, const Params& params) {
  const QMutexLocker locker(&m_mutex);

  const Item newItem(pageId, params.hardMarginsMM(), params.pageRect(), params.contentRect(), params.contentSizeMM(),
                     params.alignment(), params.isAutoMarginsEnabled());

  const Container::iterator it(m_items.find(pageId));
  if (it == m_items.end()) {
    m_items.insert(it, newItem);
  } else {
    m_items.replace(it, newItem);
  }

  m_deviationProvider.addOrUpdate(pageId);
}

Params Settings::Impl::updateContentSizeAndGetParams(const PageId& pageId,
                                                     const QRectF& pageRect,
                                                     const QRectF& contentRect,
                                                     const QSizeF& contentSizeMm,
                                                     QSizeF* aggHardSizeBefore,
                                                     QSizeF* aggHardSizeAfter) {
  const QMutexLocker locker(&m_mutex);

  if (aggHardSizeBefore) {
    *aggHardSizeBefore = getAggregateHardSizeMMLocked();
  }

  const Container::iterator it(m_items.find(pageId));
  Container::iterator itemIt(it);
  if (it == m_items.end()) {
    const Item item(pageId, m_defaultHardMarginsMM, pageRect, contentRect, contentSizeMm, m_defaultAlignment,
                    m_autoMarginsDefault);
    itemIt = m_items.insert(it, item);
  } else {
    m_items.modify(it, ModifyContentSize(contentSizeMm, contentRect, pageRect));
  }

  if (aggHardSizeAfter) {
    *aggHardSizeAfter = getAggregateHardSizeMMLocked();
  }

  m_deviationProvider.addOrUpdate(pageId);
  return Params(itemIt->hardMarginsMM, itemIt->pageRect, itemIt->contentRect, itemIt->contentSizeMM, itemIt->alignment,
                itemIt->autoMargins);
}  // Settings::Impl::updateContentSizeAndGetParams

Margins Settings::Impl::getHardMarginsMM(const PageId& pageId) const {
  const QMutexLocker locker(&m_mutex);

  const Container::iterator it(m_items.find(pageId));
  if (it == m_items.end()) {
    return m_defaultHardMarginsMM;
  } else {
    return it->hardMarginsMM;
  }
}

void Settings::Impl::setHardMarginsMM(const PageId& pageId, const Margins& marginsMm) {
  const QMutexLocker locker(&m_mutex);

  const Container::iterator it(m_items.find(pageId));
  if (it == m_items.end()) {
    const Item item(pageId, marginsMm, m_invalidRect, m_invalidRect, m_invalidSize, m_defaultAlignment,
                    m_autoMarginsDefault);
    m_items.insert(it, item);
  } else {
    m_items.modify(it, ModifyMargins(marginsMm, it->autoMargins));
  }

  m_deviationProvider.addOrUpdate(pageId);
}

Alignment Settings::Impl::getPageAlignment(const PageId& pageId) const {
  const QMutexLocker locker(&m_mutex);

  const Container::iterator it(m_items.find(pageId));
  if (it == m_items.end()) {
    return m_defaultAlignment;
  } else {
    return it->alignment;
  }
}

Settings::AggregateSizeChanged Settings::Impl::setPageAlignment(const PageId& pageId, const Alignment& alignment) {
  const QMutexLocker locker(&m_mutex);

  const QSizeF aggSizeBefore(getAggregateHardSizeMMLocked());

  const Container::iterator it(m_items.find(pageId));
  if (it == m_items.end()) {
    const Item item(pageId, m_defaultHardMarginsMM, m_invalidRect, m_invalidRect, m_invalidSize, alignment,
                    m_autoMarginsDefault);
    m_items.insert(it, item);
  } else {
    m_items.modify(it, ModifyAlignment(alignment));
  }

  m_deviationProvider.addOrUpdate(pageId);

  const QSizeF aggSizeAfter(getAggregateHardSizeMMLocked());
  if (aggSizeBefore == aggSizeAfter) {
    return AGGREGATE_SIZE_UNCHANGED;
  } else {
    return AGGREGATE_SIZE_CHANGED;
  }
}

void Settings::Impl::disableAlignmentForPages(const std::vector<PageId>& pageIds) {
  const QMutexLocker locker(&m_mutex);

  // Create a null alignment (not aligned with others)
  Alignment nullAlignment;
  nullAlignment.setNull(true);

  for (const PageId& pageId : pageIds) {
    const Container::iterator it(m_items.find(pageId));
    if (it == m_items.end()) {
      const Item item(pageId, m_defaultHardMarginsMM, m_invalidRect, m_invalidRect, m_invalidSize, nullAlignment,
                      m_autoMarginsDefault);
      m_items.insert(it, item);
    } else {
      m_items.modify(it, ModifyAlignment(nullAlignment));
    }
    m_deviationProvider.addOrUpdate(pageId);
  }
}

Settings::AggregateSizeChanged Settings::Impl::setContentSizeMM(const PageId& pageId, const QSizeF& contentSizeMm) {
  const QMutexLocker locker(&m_mutex);

  const QSizeF aggSizeBefore(getAggregateHardSizeMMLocked());

  const Container::iterator it(m_items.find(pageId));
  if (it == m_items.end()) {
    const Item item(pageId, m_defaultHardMarginsMM, m_invalidRect, m_invalidRect, contentSizeMm, m_defaultAlignment,
                    m_autoMarginsDefault);
    m_items.insert(it, item);
  } else {
    m_items.modify(it, ModifyContentSize(contentSizeMm, m_invalidRect, it->pageRect));
  }

  m_deviationProvider.addOrUpdate(pageId);

  const QSizeF aggSizeAfter(getAggregateHardSizeMMLocked());
  if (aggSizeBefore == aggSizeAfter) {
    return AGGREGATE_SIZE_UNCHANGED;
  } else {
    return AGGREGATE_SIZE_CHANGED;
  }
}

void Settings::Impl::invalidateContentSize(const PageId& pageId) {
  const QMutexLocker locker(&m_mutex);

  const Container::iterator it(m_items.find(pageId));
  if (it != m_items.end()) {
    m_items.modify(it, ModifyContentSize(m_invalidSize, m_invalidRect, it->pageRect));
  }

  m_deviationProvider.addOrUpdate(pageId);
}

QSizeF Settings::Impl::getAggregateHardSizeMM() const {
  const QMutexLocker locker(&m_mutex);
  return getAggregateHardSizeMMLocked();
}

QSizeF Settings::Impl::getAggregateHardSizeMMLocked() const {
  if (m_items.empty()) {
    return QSizeF(0.0, 0.0);
  }

  const Item& maxWidthItem = *m_descWidthOrder.begin();
  const Item& maxHeightItem = *m_descHeightOrder.begin();

  const double width = maxWidthItem.influenceHardWidthMM();
  const double height = maxHeightItem.influenceHardHeightMM();
  return QSizeF(width, height);
}

QSizeF Settings::Impl::getAggregateHardSizeMM(const PageId& pageId,
                                              const QSizeF& hardSizeMm,
                                              const Alignment& alignment) const {
  if (alignment.isNull()) {
    return getAggregateHardSizeMM();
  }

  const QMutexLocker locker(&m_mutex);

  if (m_items.empty()) {
    return QSizeF(0.0, 0.0);
  }

  double width = 0.0;

  {
    DescWidthOrder::iterator it(m_descWidthOrder.begin());
    if (it->pageId != pageId) {
      width = it->influenceHardWidthMM();
    } else {
      ++it;
      if (it == m_descWidthOrder.end()) {
        width = hardSizeMm.width();
      } else {
        width = std::max(hardSizeMm.width(), qreal(it->influenceHardWidthMM()));
      }
    }
  }

  double height = 0.0;

  {
    DescHeightOrder::iterator it(m_descHeightOrder.begin());
    if (it->pageId != pageId) {
      height = it->influenceHardHeightMM();
    } else {
      ++it;
      if (it == m_descHeightOrder.end()) {
        height = hardSizeMm.height();
      } else {
        height = std::max(hardSizeMm.height(), qreal(it->influenceHardHeightMM()));
      }
    }
  }
  return QSizeF(width, height);
}  // Settings::Impl::getAggregateHardSizeMM

bool Settings::Impl::isPageAutoMarginsEnabled(const PageId& pageId) {
  const QMutexLocker locker(&m_mutex);

  const Container::iterator it(m_items.find(pageId));
  if (it == m_items.end()) {
    return m_autoMarginsDefault;
  } else {
    return it->autoMargins;
  }
}

void Settings::Impl::setPageAutoMarginsEnabled(const PageId& pageId, const bool state) {
  const QMutexLocker locker(&m_mutex);

  const Container::iterator it(m_items.find(pageId));
  if (it == m_items.end()) {
    const Item item(pageId, m_defaultHardMarginsMM, m_invalidRect, m_invalidRect, m_invalidSize, m_defaultAlignment,
                    state);
    m_items.insert(it, item);
  } else {
    m_items.modify(it, ModifyMargins(it->hardMarginsMM, state));
  }
}

bool Settings::Impl::isParamsNull(const PageId& pageId) const {
  const QMutexLocker locker(&m_mutex);
  return (m_items.find(pageId) == m_items.end());
}

const DeviationProvider<PageId>& Settings::Impl::deviationProvider() const {
  return m_deviationProvider;
}

std::vector<Guide>& Settings::Impl::guides() {
  return m_guides;
}

bool Settings::Impl::isShowingMiddleRectEnabled() const {
  return m_showMiddleRect;
}

void Settings::Impl::enableShowingMiddleRect(const bool state) {
  m_showMiddleRect = state;
}

std::vector<Settings::PageSizeInfo> Settings::Impl::getPagesWithExcessiveSoftMargins(double threshold) const {
  std::vector<Settings::PageSizeInfo> result;

  const QMutexLocker locker(&m_mutex);

  if (m_items.empty()) {
    return result;
  }

  // Get aggregate size
  const QSizeF aggSize = getAggregateHardSizeMMLocked();
  if (aggSize.isEmpty()) {
    return result;
  }

  const double aggArea = aggSize.width() * aggSize.height();

  // Iterate over all pages
  for (const Item& item : m_unorderedItems) {
    if (!item.alignedWithOthers()) {
      continue;  // Pages not aligned don't get soft margins
    }

    const double hardWidth = item.hardWidthMM();
    const double hardHeight = item.hardHeightMM();
    const double hardArea = hardWidth * hardHeight;

    if (hardArea <= 0) {
      continue;
    }

    // Soft margin ratio is the proportion of the final page that would be soft margins
    // Final page = aggArea, hard content = hardArea, soft margins = aggArea - hardArea
    const double softMarginRatio = (aggArea - hardArea) / aggArea;

    if (softMarginRatio >= threshold) {
      Settings::PageSizeInfo info;
      info.pageId = item.pageId;
      info.hardWidthMM = hardWidth;
      info.hardHeightMM = hardHeight;
      info.softMarginRatio = softMarginRatio;
      result.push_back(info);
    }
  }

  // Sort by soft margin ratio descending (worst offenders first)
  std::sort(result.begin(), result.end(), [](const Settings::PageSizeInfo& a, const Settings::PageSizeInfo& b) {
    return a.softMarginRatio > b.softMarginRatio;
  });

  return result;
}

std::vector<Settings::OutlierPageInfo> Settings::Impl::getOutlierPages(double deviationThreshold) const {
  std::vector<Settings::OutlierPageInfo> result;

  const QMutexLocker locker(&m_mutex);

  if (m_items.empty()) {
    qDebug() << "getOutlierPages: m_items is empty";
    return result;
  }

  // Collect all aligned page sizes (area = width * height)
  std::vector<double> widths;
  std::vector<double> heights;
  std::vector<const Item*> alignedItems;

  int skippedNotAligned = 0;
  int skippedInvalidSize = 0;
  for (const Item& item : m_unorderedItems) {
    if (!item.alignedWithOthers()) {
      skippedNotAligned++;
      continue;  // Skip pages not aligned
    }
    const double w = item.hardWidthMM();
    const double h = item.hardHeightMM();
    if (w <= 0 || h <= 0) {
      skippedInvalidSize++;
      continue;
    }
    widths.push_back(w);
    heights.push_back(h);
    alignedItems.push_back(&item);
  }

  qDebug() << "getOutlierPages: total items:" << m_items.size()
           << "aligned:" << alignedItems.size()
           << "skippedNotAligned:" << skippedNotAligned
           << "skippedInvalidSize:" << skippedInvalidSize;

  if (alignedItems.size() < 2) {
    qDebug() << "getOutlierPages: Not enough aligned items (<2)";
    return result;  // Need at least 2 pages to find outliers
  }

  // Calculate median width and height
  std::sort(widths.begin(), widths.end());
  std::sort(heights.begin(), heights.end());

  const size_t n = widths.size();
  double medianWidth, medianHeight;
  if (n % 2 == 0) {
    medianWidth = (widths[n / 2 - 1] + widths[n / 2]) / 2.0;
    medianHeight = (heights[n / 2 - 1] + heights[n / 2]) / 2.0;
  } else {
    medianWidth = widths[n / 2];
    medianHeight = heights[n / 2];
  }

  // Get aggregate size to identify which pages set it
  const QSizeF aggSize = getAggregateHardSizeMMLocked();
  const double aggWidth = aggSize.width();
  const double aggHeight = aggSize.height();

  qDebug() << "getOutlierPages: medianWidth:" << medianWidth << "medianHeight:" << medianHeight
           << "aggWidth:" << aggWidth << "aggHeight:" << aggHeight
           << "minWidth:" << widths.front() << "maxWidth:" << widths.back()
           << "minHeight:" << heights.front() << "maxHeight:" << heights.back();

  // Find outlier pages (deviation from median)
  int outlierCount = 0;
  for (const Item* item : alignedItems) {
    const double w = item->hardWidthMM();
    const double h = item->hardHeightMM();
    const double area = w * h;
    const double medianArea = medianWidth * medianHeight;

    // Calculate deviation ratio (how much larger or smaller)
    const double deviationRatio = area / medianArea;

    // Check if this is an outlier (either much larger or much smaller)
    const bool isLarger = deviationRatio > 1.0;
    const bool isOutlier = isLarger ? (deviationRatio >= deviationThreshold)
                                     : (deviationRatio <= 1.0 / deviationThreshold);

    if (isOutlier) {
      outlierCount++;
      Settings::OutlierPageInfo info;
      info.pageId = item->pageId;
      info.hardWidthMM = w;
      info.hardHeightMM = h;
      info.medianWidthMM = medianWidth;
      info.medianHeightMM = medianHeight;
      info.deviationRatio = deviationRatio;
      info.isLarger = isLarger;
      // Check if this page sets the aggregate size (within tolerance)
      info.setsAggregateWidth = (std::abs(w - aggWidth) < 0.01);
      info.setsAggregateHeight = (std::abs(h - aggHeight) < 0.01);
      result.push_back(info);
    }
  }

  qDebug() << "getOutlierPages: found" << outlierCount << "outliers with threshold" << deviationThreshold;

  // Sort by deviation (largest outliers first - both bigger and smaller extremes)
  std::sort(result.begin(), result.end(), [](const Settings::OutlierPageInfo& a, const Settings::OutlierPageInfo& b) {
    // Sort by how far from 1.0 the deviation is
    const double devA = a.deviationRatio > 1.0 ? a.deviationRatio : 1.0 / a.deviationRatio;
    const double devB = b.deviationRatio > 1.0 ? b.deviationRatio : 1.0 / b.deviationRatio;
    return devA > devB;
  });

  return result;
}

std::vector<Settings::OutlierPageInfo> Settings::Impl::getUnsplitSpreadPages() const {
  std::vector<Settings::OutlierPageInfo> result;

  const QMutexLocker locker(&m_mutex);

  if (m_items.empty()) {
    return result;
  }

  // Collect all page widths to find the median (typical single page width)
  std::vector<double> widths;
  std::vector<const Item*> allItems;

  for (const Item& item : m_unorderedItems) {
    if (!item.alignedWithOthers()) {
      continue;
    }
    const double w = item.hardWidthMM();
    if (w <= 0) {
      continue;
    }
    widths.push_back(w);
    allItems.push_back(&item);
  }

  if (widths.size() < 2) {
    return result;
  }

  // Sort widths to find median
  std::vector<double> sortedWidths = widths;
  std::sort(sortedWidths.begin(), sortedWidths.end());

  const size_t n = sortedWidths.size();
  double medianWidth;
  if (n % 2 == 0) {
    medianWidth = (sortedWidths[n / 2 - 1] + sortedWidths[n / 2]) / 2.0;
  } else {
    medianWidth = sortedWidths[n / 2];
  }

  // Also get aggregate width
  const QSizeF aggSize = getAggregateHardSizeMMLocked();
  const double aggWidth = aggSize.width();

  // Check if aggregate width suggests spreads (aggregate ~2x median)
  const double aggToMedianRatio = medianWidth > 0 ? (aggWidth / medianWidth) : 1.0;
  const bool aggLooksSpreads = (aggToMedianRatio > 1.8 && aggToMedianRatio < 2.2);

  if (!aggLooksSpreads) {
    // Aggregate doesn't look like spreads are involved
    return result;
  }

  // The expected single-page width is approximately half the aggregate
  const double expectedSinglePageWidth = aggWidth / 2.0;

  qDebug() << "getUnsplitSpreadPages: aggWidth:" << aggWidth
           << "medianWidth:" << medianWidth
           << "expectedSinglePageWidth:" << expectedSinglePageWidth;

  // Find pages whose width is close to the aggregate (i.e., unsplit spreads)
  // Pages that are properly split will have width close to expectedSinglePageWidth
  for (size_t i = 0; i < allItems.size(); ++i) {
    const Item* item = allItems[i];
    const double w = item->hardWidthMM();
    const double h = item->hardHeightMM();

    // Check if this page's width is close to aggregate width (unsplit spread)
    // rather than close to expected single page width (properly split)
    const double widthRatioToAgg = w / aggWidth;
    const double widthRatioToSingle = w / expectedSinglePageWidth;

    // If width is within 10% of aggregate width, it's likely an unsplit spread
    const bool looksLikeSpread = (widthRatioToAgg > 0.9 && widthRatioToAgg < 1.1);

    if (looksLikeSpread) {
      Settings::OutlierPageInfo info;
      info.pageId = item->pageId;
      info.hardWidthMM = w;
      info.hardHeightMM = h;
      info.medianWidthMM = expectedSinglePageWidth;  // Expected single page width
      info.medianHeightMM = h;  // Height stays the same
      info.deviationRatio = widthRatioToSingle;  // How much wider than a single page
      info.isLarger = true;
      info.setsAggregateWidth = (std::abs(w - aggWidth) < 0.01);
      info.setsAggregateHeight = (std::abs(h - aggSize.height()) < 0.01);
      result.push_back(info);
    }
  }

  qDebug() << "getUnsplitSpreadPages: found" << result.size() << "unsplit spread pages";

  return result;
}
}  // namespace page_layout