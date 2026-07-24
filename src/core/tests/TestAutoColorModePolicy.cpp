#include <boost/test/unit_test.hpp>
#include <QDomDocument>

#include "ImageId.h"
#include "PageId.h"
#include "filters/finalize/Settings.h"
#include "filters/output/ColorParams.h"

BOOST_AUTO_TEST_SUITE(AutoColorModePolicyTestSuite)

BOOST_AUTO_TEST_CASE(never_color_maps_only_color_to_grayscale) {
  using finalize::AutoColorModePolicy;
  using finalize::ColorMode;
  BOOST_CHECK(applyAutoColorModePolicy(ColorMode::Color, AutoColorModePolicy::NeverColor)
              == ColorMode::Grayscale);
  BOOST_CHECK(applyAutoColorModePolicy(ColorMode::BlackAndWhite, AutoColorModePolicy::NeverColor)
              == ColorMode::BlackAndWhite);
  BOOST_CHECK(applyAutoColorModePolicy(ColorMode::Grayscale, AutoColorModePolicy::NeverColor)
              == ColorMode::Grayscale);
}

BOOST_AUTO_TEST_CASE(force_black_and_white_maps_every_verdict) {
  using finalize::AutoColorModePolicy;
  using finalize::ColorMode;
  BOOST_CHECK(applyAutoColorModePolicy(ColorMode::Color, AutoColorModePolicy::ForceBlackAndWhite)
              == ColorMode::BlackAndWhite);
  BOOST_CHECK(applyAutoColorModePolicy(ColorMode::Grayscale, AutoColorModePolicy::ForceBlackAndWhite)
              == ColorMode::BlackAndWhite);
}

BOOST_AUTO_TEST_CASE(best_guess_recovers_the_raw_color_verdict) {
  using finalize::AutoColorModePolicy;
  using finalize::ColorMode;
  const ColorMode rawVerdict = ColorMode::Color;
  BOOST_CHECK(applyAutoColorModePolicy(rawVerdict, AutoColorModePolicy::NeverColor)
              == ColorMode::Grayscale);
  BOOST_CHECK(applyAutoColorModePolicy(rawVerdict, AutoColorModePolicy::BestGuess)
              == ColorMode::Color);
}

BOOST_AUTO_TEST_CASE(force_preset_provenance_is_distinct_from_manual_choice) {
  output::ColorParams params;
  params.setColorMode(output::BLACK_AND_WHITE);
  params.setColorModeUserSet(true);
  params.setColorModePresetSet(true);
  BOOST_CHECK(params.isColorModeUserSet());
  BOOST_CHECK(params.isColorModePresetSet());

  QDomDocument document;
  const output::ColorParams restored(params.toXml(document, QStringLiteral("color-params")));
  BOOST_CHECK(restored.isColorModeUserSet());
  BOOST_CHECK(restored.isColorModePresetSet());

  output::ColorParams manuallyChanged = restored;
  manuallyChanged.setColorMode(output::COLOR);
  manuallyChanged.setColorModeUserSet(true);
  BOOST_CHECK(manuallyChanged.isColorModeUserSet());
  BOOST_CHECK(!manuallyChanged.isColorModePresetSet());
}

BOOST_AUTO_TEST_CASE(clear_detection_cache_makes_every_seeded_page_undecided) {
  finalize::Settings finalizeSettings;
  constexpr int pageCount = 4;

  for (int page = 1; page <= pageCount; ++page) {
    const PageId pageId(ImageId(QStringLiteral("/tmp/redetect-page.tif"), page));
    finalizeSettings.setColorMode(pageId, finalize::ColorMode::Color);
    finalizeSettings.setProcessed(pageId, true);
    BOOST_CHECK(!finalizeSettings.isColorModeDetectionNeeded(pageId));
  }

  finalizeSettings.clearDetectionCache();

  for (int page = 1; page <= pageCount; ++page) {
    const PageId pageId(ImageId(QStringLiteral("/tmp/redetect-page.tif"), page));
    BOOST_CHECK(finalizeSettings.isColorModeDetectionNeeded(pageId));
    BOOST_CHECK(!finalizeSettings.isProcessed(pageId));
  }
}

BOOST_AUTO_TEST_SUITE_END()
