// Copyright (C) 2026 ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include <QAbstractButton>

#include <boost/test/unit_test.hpp>

#include <memory>
#include <set>
#include <utility>

#include "ImageMetadata.h"
#include "PageInfo.h"
#include "PageSelectionAccessor.h"
#include "PageSelectionProvider.h"
#include "PageSequence.h"
#include "filters/select_content/OptionsWidget.h"
#include "filters/select_content/Settings.h"

namespace Tests {
namespace {

class FixedSelectionProvider final : public PageSelectionProvider {
 public:
  explicit FixedSelectionProvider(PageSequence pages) : m_pages(std::move(pages)) {}

  PageSequence allPages() const override { return m_pages; }

  std::set<PageId> selectedPages() const override {
    std::set<PageId> selected;
    for (const PageInfo& page : m_pages) {
      selected.insert(page.id());
    }
    return selected;
  }

  std::vector<PageRange> selectedRanges() const override { return {}; }

 private:
  PageSequence m_pages;
};

PageInfo makePage(const QString& path) {
  return PageInfo(PageId(ImageId(path)), ImageMetadata(QSize(1000, 1500), Dpi(300, 300)), 1, false, false);
}

PageSequence makePages(const PageInfo& current, const PageInfo& other1, const PageInfo& other2) {
  PageSequence pages;
  pages.append(current);
  pages.append(other1);
  pages.append(other2);
  return pages;
}

select_content::OptionsWidget::UiData makeUiData() {
  select_content::OptionsWidget::UiData data;
  data.setContentRect(QRectF(100, 100, 700, 1100));
  data.setPageRect(QRectF(0, 0, 1000, 1500));
  data.setContentDetectionMode(MODE_AUTO);
  data.setPageDetectionMode(MODE_DISABLED);
  data.setFineTuneCornersEnabled(false);
  return data;
}

struct InvalidationRecorder {
  std::set<PageId> pages;
  int allCount = 0;
};

}  // namespace

BOOST_AUTO_TEST_SUITE(ThumbnailInvalidationTestSuite)

BOOST_AUTO_TEST_CASE(manual_content_box_edit_invalidates_only_the_current_page) {
  const PageInfo current = makePage(QStringLiteral("/tmp/current.png"));
  const PageInfo other1 = makePage(QStringLiteral("/tmp/other-1.png"));
  const PageInfo other2 = makePage(QStringLiteral("/tmp/other-2.png"));
  const PageSequence pages = makePages(current, other1, other2);
  const PageSelectionAccessor accessor(std::make_shared<FixedSelectionProvider>(pages));
  auto settings = std::make_shared<select_content::Settings>();
  select_content::OptionsWidget options(settings, accessor);
  options.preUpdateUI(current);
  options.postUpdateUI(makeUiData());

  InvalidationRecorder recorder;
  QObject::connect(&options,
                   qOverload<const PageId&>(&FilterOptionsWidget::invalidateThumbnail),
                   [&recorder](const PageId& pageId) { recorder.pages.insert(pageId); });
  QObject::connect(&options,
                   &FilterOptionsWidget::invalidateAllThumbnails,
                   [&recorder]() { ++recorder.allCount; });

  options.manualContentRectSet(QRectF(120, 130, 680, 1080));

  BOOST_CHECK_EQUAL(recorder.allCount, 0);
  BOOST_CHECK_EQUAL(recorder.pages.size(), 1);
  BOOST_CHECK(recorder.pages == std::set<PageId>{current.id()});
}

BOOST_AUTO_TEST_CASE(selected_page_mode_edit_invalidates_exactly_the_written_scope) {
  const PageInfo current = makePage(QStringLiteral("/tmp/current.png"));
  const PageInfo other1 = makePage(QStringLiteral("/tmp/other-1.png"));
  const PageInfo other2 = makePage(QStringLiteral("/tmp/other-2.png"));
  const PageSequence pages = makePages(current, other1, other2);
  const PageSelectionAccessor accessor(std::make_shared<FixedSelectionProvider>(pages));
  auto settings = std::make_shared<select_content::Settings>();
  select_content::OptionsWidget options(settings, accessor);
  options.preUpdateUI(current);
  options.postUpdateUI(makeUiData());

  InvalidationRecorder recorder;
  QObject::connect(&options,
                   qOverload<const PageId&>(&FilterOptionsWidget::invalidateThumbnail),
                   [&recorder](const PageId& pageId) { recorder.pages.insert(pageId); });
  QObject::connect(&options,
                   &FilterOptionsWidget::invalidateAllThumbnails,
                   [&recorder]() { ++recorder.allCount; });

  QAbstractButton* manualButton = options.findChild<QAbstractButton*>(QStringLiteral("contentDetectManualBtn"));
  BOOST_REQUIRE(manualButton != nullptr);
  manualButton->click();

  BOOST_CHECK_EQUAL(recorder.allCount, 0);
  BOOST_CHECK((recorder.pages == std::set<PageId>{current.id(), other1.id(), other2.id()}));
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace Tests
