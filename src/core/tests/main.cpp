// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define BOOST_TEST_NO_MAIN

#include <boost/test/unit_test.hpp>

#include <QApplication>
#include <QSettings>
#include <QTemporaryDir>

#include <cstdlib>

namespace {

bool initUnitTest() {
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  setenv("QT_QPA_PLATFORM", "offscreen", /*overwrite=*/0);

  QTemporaryDir settingsDir;
  QApplication::setOrganizationName(QStringLiteral("ScanTailorSpectreTests"));
  QApplication::setApplicationName(QStringLiteral("core_tests"));
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());

  int qtArgc = 1;
  char arg0[] = "core_tests";
  char* qtArgv[] = {arg0, nullptr};
  QApplication app(qtArgc, qtArgv);

  return boost::unit_test::unit_test_main(&initUnitTest, argc, argv);
}
