// Copyright (C) 2026  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "ApplyLayoutType.h"

#include "PageLayoutAdapter.h"
#include "PageSequence.h"
#include "Params.h"
#include "Settings.h"

namespace page_split {
namespace {
std::set<PageId> everyOther(const std::set<PageId>& pages) {
  std::set<PageId> result;
  int index = 0;
  for (const PageId& page : pages) {
    if (index % 2 == 0) {
      result.insert(page);
    }
    ++index;
  }
  return result;
}
}  // namespace

std::set<PageId> pagesForLayoutTypeScope(const PageSequence& pages,
                                         const std::set<PageId>& selectedPages,
                                         const PageId& currentPage,
                                         const LayoutTypeScope scope) {
  switch (scope) {
    case LayoutTypeScope::THIS_PAGE:
      return {currentPage};
    case LayoutTypeScope::ALL_PAGES:
      return pages.selectAll();
    case LayoutTypeScope::THIS_PAGE_AND_FOLLOWERS:
      return pages.selectPagePlusFollowers(currentPage);
    case LayoutTypeScope::THIS_PAGE_AND_FOLLOWING_EVERY_OTHER:
      return everyOther(pages.selectPagePlusFollowers(currentPage));
    case LayoutTypeScope::EVERY_OTHER:
      return pages.selectEveryOther(currentPage);
    case LayoutTypeScope::SELECTED_PAGES:
      return selectedPages;
    case LayoutTypeScope::EVERY_OTHER_SELECTED:
      return everyOther(selectedPages);
  }

  return {};
}

void applyManualSplitToScope(Settings& settings,
                             const std::set<PageId>& pages,
                             const Params& sourceParams,
                             const LayoutType layoutType) {
  for (const PageId& pageId : pages) {
    Settings::UpdateAction update;
    update.setLayoutType(layoutType);

    Params newPageParams(sourceParams);
    const Settings::Record oldPageRecord = settings.getPageRecord(pageId.imageId());
    const Params* oldPageParams = oldPageRecord.params();
    if (oldPageParams) {
      const PageLayout newPageLayout = PageLayoutAdapter::adaptPageLayout(
          sourceParams.pageLayout(), oldPageParams->pageLayout().uncutOutline().boundingRect());

      update.setLayoutType(newPageLayout.toLayoutType());
      newPageParams.setPageLayout(newPageLayout);

      Dependencies dependencies = oldPageParams->dependencies();
      dependencies.setLayoutType(newPageLayout.toLayoutType());
      newPageParams.setDependencies(dependencies);
    }
    update.setParams(newPageParams);
    settings.updatePage(pageId.imageId(), update);
  }
}
}  // namespace page_split
