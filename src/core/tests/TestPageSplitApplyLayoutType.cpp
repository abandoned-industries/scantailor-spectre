// Copyright (C) 2026  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include <boost/test/unit_test.hpp>

#include <QDomDocument>
#include <QLineF>
#include <QRectF>

#include <vector>

#include "AutoManualMode.h"
#include "Dpi.h"
#include "ImageId.h"
#include "ImageMetadata.h"
#include "OrthogonalRotation.h"
#include "PageInfo.h"
#include "PageSequence.h"
#include "filters/page_split/ApplyLayoutType.h"
#include "filters/page_split/Dependencies.h"
#include "filters/page_split/PageLayout.h"
#include "filters/page_split/PageLayoutAdapter.h"
#include "filters/page_split/Params.h"
#include "filters/page_split/Settings.h"
#include "filters/page_split/SplitModeDialogMode.h"

namespace page_split {
namespace {
struct ScopeCase {
  LayoutTypeScope scope;
  std::set<int> expectedIndexes;
};

std::vector<PageId> makePages(PageSequence& sequence, const int count = 6) {
  std::vector<PageId> pages;
  for (int i = 0; i < count; ++i) {
    const PageId page(ImageId(QString("page-%1.tif").arg(i)));
    pages.push_back(page);
    sequence.append(PageInfo(page, ImageMetadata(QSize(100, 200), Dpi(300, 300)), 1, false, false));
  }
  return pages;
}

std::set<PageId> pageSet(const std::vector<PageId>& pages, const std::set<int>& indexes) {
  std::set<PageId> result;
  for (const int index : indexes) {
    result.insert(pages.at(index));
  }
  return result;
}

bool sameLayout(const PageLayout& lhs, const PageLayout& rhs) {
  if ((lhs.type() != rhs.type()) || (lhs.uncutOutline() != rhs.uncutOutline())
      || (lhs.numCutters() != rhs.numCutters())) {
    return false;
  }
  for (int i = 0; i < lhs.numCutters(); ++i) {
    if (lhs.cutterLine(i) != rhs.cutterLine(i)) {
      return false;
    }
  }
  return true;
}

QString dependenciesXml(const Dependencies& dependencies) {
  QDomDocument document;
  document.appendChild(dependencies.toXml(document, QStringLiteral("dependencies")));
  return document.toString();
}

PageLayout sourceLayout(const LayoutType layoutType, const QRectF& outline) {
  if (layoutType == TWO_PAGES) {
    return PageLayout(outline, QLineF(45, 10, 75, 210));
  }
  return PageLayout(outline, QLineF(20, 10, 40, 210), QLineF(90, 10, 100, 210));
}

void seedPage(Settings& settings,
              const PageId& page,
              const PageLayout& layout,
              const QSize& imageSize,
              const LayoutType dependencyLayoutType,
              const AutoManualMode mode,
              const QString& reason) {
  const Params params(layout, Dependencies(imageSize, OrthogonalRotation(), dependencyLayoutType), mode, reason);
  Settings::UpdateAction update;
  update.setParams(params);
  settings.updatePage(page.imageId(), update);
}
}  // namespace

BOOST_AUTO_TEST_CASE(layout_type_scope_resolves_all_seven_scopes) {
  PageSequence sequence;
  const std::vector<PageId> pages = makePages(sequence);
  const PageId currentPage = pages.at(2);
  const std::set<PageId> selectedPages = pageSet(pages, {0, 2, 3, 5});
  const std::vector<ScopeCase> cases = {
      {LayoutTypeScope::THIS_PAGE, {2}},
      {LayoutTypeScope::ALL_PAGES, {0, 1, 2, 3, 4, 5}},
      {LayoutTypeScope::THIS_PAGE_AND_FOLLOWERS, {2, 3, 4, 5}},
      {LayoutTypeScope::THIS_PAGE_AND_FOLLOWING_EVERY_OTHER, {2, 4}},
      {LayoutTypeScope::EVERY_OTHER, {0, 2, 4}},
      {LayoutTypeScope::SELECTED_PAGES, {0, 2, 3, 5}},
      {LayoutTypeScope::EVERY_OTHER_SELECTED, {0, 3}},
  };

  for (const ScopeCase& scopeCase : cases) {
    const std::set<PageId> resolvedPages
        = pagesForLayoutTypeScope(sequence, selectedPages, currentPage, scopeCase.scope);
    BOOST_CHECK(resolvedPages == pageSet(pages, scopeCase.expectedIndexes));
  }
}

BOOST_AUTO_TEST_CASE(split_mode_dialog_opens_manual_for_either_manual_signal) {
  BOOST_TEST(!splitModeDialogOpensManual(AUTO_LAYOUT_TYPE, MODE_AUTO));
  BOOST_TEST(splitModeDialogOpensManual(AUTO_LAYOUT_TYPE, MODE_MANUAL));
  BOOST_TEST(splitModeDialogOpensManual(TWO_PAGES, MODE_AUTO));
  BOOST_TEST(splitModeDialogOpensManual(TWO_PAGES, MODE_MANUAL));
}

BOOST_AUTO_TEST_CASE(manual_split_is_fitted_to_every_target_without_clearing_params) {
  PageSequence sequence;
  const std::vector<PageId> pages = makePages(sequence, 4);
  const std::vector<QRectF> outlines = {
      QRectF(-10, -40, 180, 320),
      QRectF(0, 10, 120, 200),
      QRectF(10, 40, 150, 100),
      QRectF(-20, -100, 220, 500),
  };
  const std::set<PageId> scope = pageSet(pages, {0, 1, 2});

  for (const LayoutType layoutType : {PAGE_PLUS_OFFCUT, TWO_PAGES}) {
    Settings settings;
    for (int i = 0; i < 4; ++i) {
      seedPage(settings, pages.at(i), PageLayout(outlines.at(i)), QSize(100 + i, 200 + i), AUTO_LAYOUT_TYPE,
               MODE_AUTO, QString("original-%1").arg(i));
    }

    const PageLayout drawnLayout = sourceLayout(layoutType, outlines.at(1));
    seedPage(settings, pages.at(1), drawnLayout, QSize(101, 201), AUTO_LAYOUT_TYPE, MODE_MANUAL,
             QStringLiteral("drawn-source"));

    const Params sourceParams = *settings.getPageRecord(pages.at(1).imageId()).params();
    const Params outsideBefore = *settings.getPageRecord(pages.at(3).imageId()).params();
    applyManualSplitToScope(settings, scope, sourceParams, layoutType);

    for (int i = 0; i < 3; ++i) {
      const Settings::Record record = settings.getPageRecord(pages.at(i).imageId());
      BOOST_REQUIRE(record.params() != nullptr);

      const PageLayout expectedLayout = PageLayoutAdapter::adaptPageLayout(drawnLayout, outlines.at(i));
      BOOST_CHECK(sameLayout(record.params()->pageLayout(), expectedLayout));
      BOOST_TEST(record.combinedLayoutType() == layoutType);

      Dependencies expectedDependencies(QSize(100 + i, 200 + i), OrthogonalRotation(), layoutType);
      BOOST_CHECK(dependenciesXml(record.params()->dependencies()) == dependenciesXml(expectedDependencies));

      if (i != 1) {
        BOOST_CHECK(record.params()->pageLayout().cutterLine(0) != drawnLayout.cutterLine(0));
      }
    }

    const Settings::Record sourceRecord = settings.getPageRecord(pages.at(1).imageId());
    const Params* sourceAfter = sourceRecord.params();
    BOOST_REQUIRE(sourceAfter != nullptr);
    BOOST_CHECK(sameLayout(sourceAfter->pageLayout(), drawnLayout));
    BOOST_TEST(sourceAfter->splitLineMode() == MODE_MANUAL);
    BOOST_CHECK(sourceAfter->decisionReason() == QStringLiteral("drawn-source"));

    const Settings::Record outsideAfter = settings.getPageRecord(pages.at(3).imageId());
    BOOST_REQUIRE(outsideAfter.params() != nullptr);
    BOOST_CHECK(sameLayout(outsideAfter.params()->pageLayout(), outsideBefore.pageLayout()));
    BOOST_CHECK(dependenciesXml(outsideAfter.params()->dependencies())
                == dependenciesXml(outsideBefore.dependencies()));
    BOOST_TEST(outsideAfter.params()->splitLineMode() == outsideBefore.splitLineMode());
    BOOST_CHECK(outsideAfter.params()->decisionReason() == outsideBefore.decisionReason());
    BOOST_TEST(outsideAfter.combinedLayoutType() == AUTO_LAYOUT_TYPE);
  }
}
}  // namespace page_split
