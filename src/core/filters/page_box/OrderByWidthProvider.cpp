// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "OrderByWidthProvider.h"

#include <utility>

namespace page_box {
OrderByWidthProvider::OrderByWidthProvider(std::shared_ptr<Settings> settings) : m_settings(std::move(settings)) {}

bool OrderByWidthProvider::precedes(const PageId& lhsPage,
                                    const bool lhsIncomplete,
                                    const PageId& rhsPage,
                                    const bool rhsIncomplete) const {
  const std::unique_ptr<Params> lhsParams(m_settings->getPageParams(lhsPage));
  const std::unique_ptr<Params> rhsParams(m_settings->getPageParams(rhsPage));

  QRectF lhsRect;
  if (lhsParams) {
    lhsRect = lhsParams->pageRect();
  }
  QRectF rhsRect;
  if (rhsParams) {
    rhsRect = rhsParams->pageRect();
  }

  const bool lhsValid = !lhsIncomplete && lhsRect.isValid();
  const bool rhsValid = !rhsIncomplete && rhsRect.isValid();

  if (lhsValid != rhsValid) {
    return lhsValid;
  }
  return lhsRect.width() < rhsRect.width();
}
}  // namespace page_box
