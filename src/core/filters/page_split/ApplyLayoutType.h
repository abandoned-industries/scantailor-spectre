// Copyright (C) 2026  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_PAGE_SPLIT_APPLYLAYOUTTYPE_H_
#define SCANTAILOR_PAGE_SPLIT_APPLYLAYOUTTYPE_H_

#include <set>

#include "LayoutType.h"
#include "PageId.h"

class PageSequence;

namespace page_split {
class Settings;
class Params;

enum class LayoutTypeScope {
  THIS_PAGE,
  ALL_PAGES,
  THIS_PAGE_AND_FOLLOWERS,
  THIS_PAGE_AND_FOLLOWING_EVERY_OTHER,
  EVERY_OTHER,
  SELECTED_PAGES,
  EVERY_OTHER_SELECTED
};

std::set<PageId> pagesForLayoutTypeScope(const PageSequence& pages,
                                         const std::set<PageId>& selectedPages,
                                         const PageId& currentPage,
                                         LayoutTypeScope scope);

void applyManualSplitToScope(Settings& settings,
                             const std::set<PageId>& pages,
                             const Params& sourceParams,
                             LayoutType layoutType);
}  // namespace page_split

#endif  // ifndef SCANTAILOR_PAGE_SPLIT_APPLYLAYOUTTYPE_H_
