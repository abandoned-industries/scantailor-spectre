#include <boost/test/unit_test.hpp>

#include "DurationFormatter.h"

BOOST_AUTO_TEST_SUITE(DurationFormatterTestSuite)

BOOST_AUTO_TEST_CASE(formats_sub_minute_durations_with_tenths) {
  BOOST_CHECK_EQUAL(core::formatDuration(0).toStdString(), "0.0 s");
  BOOST_CHECK_EQUAL(core::formatDuration(42300).toStdString(), "42.3 s");
  BOOST_CHECK_EQUAL(core::formatDuration(59900).toStdString(), "59.9 s");
}

BOOST_AUTO_TEST_CASE(formats_minute_durations_with_total_seconds) {
  BOOST_CHECK_EQUAL(core::formatDuration(60000).toStdString(), "1 m 0 s (60 s)");
  BOOST_CHECK_EQUAL(core::formatDuration(272000).toStdString(), "4 m 32 s (272 s)");
  BOOST_CHECK_EQUAL(core::formatDuration(3599000).toStdString(), "59 m 59 s (3599 s)");
}

BOOST_AUTO_TEST_CASE(formats_hour_durations_without_seconds_component) {
  BOOST_CHECK_EQUAL(core::formatDuration(3600000).toStdString(), "1 h 0 m (3600 s)");
  BOOST_CHECK_EQUAL(core::formatDuration(4320000).toStdString(), "1 h 12 m (4320 s)");
}

BOOST_AUTO_TEST_CASE(clamps_negative_input_to_zero) {
  BOOST_CHECK_EQUAL(core::formatDuration(-1).toStdString(), "0.0 s");
}

BOOST_AUTO_TEST_SUITE_END()
