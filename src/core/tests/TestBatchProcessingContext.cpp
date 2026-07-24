// Copyright (C) 2026  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include <BatchProcessingContext.h>

#include <boost/test/unit_test.hpp>

namespace Tests {

BOOST_AUTO_TEST_SUITE(BatchProcessingContextTestSuite)

BOOST_AUTO_TEST_CASE(scopes_track_only_active_batch_tasks) {
  BOOST_CHECK(!batch_processing::isActive());
  BOOST_CHECK_EQUAL(batch_processing::activeTaskCount(), 0);

  {
    const batch_processing::TaskScope inactive(false);
    BOOST_CHECK(!batch_processing::isActive());

    const batch_processing::TaskScope first(true);
    BOOST_CHECK(batch_processing::isActive());
    BOOST_CHECK_EQUAL(batch_processing::activeTaskCount(), 1);

    {
      const batch_processing::TaskScope second(true);
      BOOST_CHECK_EQUAL(batch_processing::activeTaskCount(), 2);
    }

    BOOST_CHECK_EQUAL(batch_processing::activeTaskCount(), 1);
  }

  BOOST_CHECK(!batch_processing::isActive());
  BOOST_CHECK_EQUAL(batch_processing::activeTaskCount(), 0);
}

BOOST_AUTO_TEST_SUITE_END()
}  // namespace Tests
