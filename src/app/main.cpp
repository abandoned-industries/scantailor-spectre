// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include <config.h>
#include <core/Application.h>
#include <core/ApplicationSettings.h>
#include <core/ColorSchemeFactory.h>
#include <core/ColorSchemeManager.h>
#include <core/FontIconPack.h>
#include <core/IconProvider.h>
#include <core/StyledIconPack.h>

#include <QSettings>
#include <QStringList>

#include "AppController.h"
#include "MainWindow.h"

#ifdef Q_OS_MAC
#include "MetalLifecycle.h"
#endif

int main(int argc, char* argv[]) {
  // Qt6 enables high DPI scaling by default
  Application app(argc, argv);

#ifdef Q_OS_MAC
  // Initialize Metal lifecycle observer to detect app backgrounding
  // This must be done after QApplication is created but before any Metal operations
  metalLifecycleInit();

  // macOS style: don't quit when last window is closed, only on Cmd+Q or Quit menu
  Application::setQuitOnLastWindowClosed(false);
#endif

#ifdef _WIN32
  // Get rid of all references to Qt's installation directory.
  Application::setLibraryPaths(QStringList(Application::applicationDirPath()));
#endif

  QStringList args = Application::arguments();

  // This information is used by QSettings.
  Application::setApplicationName(APPLICATION_NAME);
  Application::setOrganizationName(ORGANIZATION_NAME);

  QSettings::setDefaultFormat(QSettings::IniFormat);
  if (app.isPortableVersion()) {
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, app.getPortableConfigPath());
  }

  app.installLanguage(ApplicationSettings::getInstance().getLanguage());

  {
    std::unique_ptr<ColorScheme> scheme
        = ColorSchemeFactory().create(ApplicationSettings::getInstance().getColorScheme());
    ColorSchemeManager::instance().setColorScheme(*scheme);
  }
  IconProvider::getInstance().setIconPack(StyledIconPack::createDefault());

#ifdef Q_OS_MAC
  // macOS: Use AppController to manage StartupWindow and MainWindow
  AppController controller;
  if (args.size() > 1) {
    controller.openProject(args.at(1));
  } else {
    controller.start();
  }
#else
  // Other platforms: Show MainWindow directly (original behavior)
  auto* mainWnd = new MainWindow();
  mainWnd->setAttribute(Qt::WA_DeleteOnClose);
  QSettings settings;
  if (settings.value("mainWindow/maximized", false).toBool()) {
    QTimer::singleShot(0, mainWnd, &QMainWindow::showMaximized);
  } else {
    mainWnd->show();
  }
  if (args.size() > 1) {
    mainWnd->openProject(args.at(1));
  }
#endif

  return Application::exec();
}  // main
