// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "StartupWindow.h"

#include <QApplication>
#include <QDialog>
#include <QMenuBar>
#include <QResource>
#include <QShortcut>
#include <QVBoxLayout>

#include "NewOpenProjectPanel.h"
#include "ui_AboutDialog.h"
#include "version.h"

StartupWindow::StartupWindow(QWidget* parent) : QWidget(parent) {
  setWindowTitle(tr("ScanTailor Spectre"));
  setWindowFlags(Qt::Window);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  m_panel = new NewOpenProjectPanel(this);
  layout->addWidget(m_panel);

  // Forward signals from the panel
  connect(m_panel, &NewOpenProjectPanel::openProject, this, &StartupWindow::openProjectRequested);
  connect(m_panel, &NewOpenProjectPanel::importPdf, this, &StartupWindow::importPdfRequested);
  connect(m_panel, &NewOpenProjectPanel::importFolder, this, &StartupWindow::importFolderRequested);
  connect(m_panel, &NewOpenProjectPanel::openRecentProject, this, &StartupWindow::recentProjectRequested);

#ifdef Q_OS_MAC
  // Create a menu bar for the About menu (macOS puts About in app menu automatically)
  m_menuBar = new QMenuBar(nullptr);  // nullptr makes it a global menu bar on macOS
  auto* helpMenu = m_menuBar->addMenu(tr("&Help"));
  auto* aboutAction = helpMenu->addAction(tr("About ScanTailor Spectre"));
  aboutAction->setMenuRole(QAction::AboutRole);  // This moves it to the app menu on macOS
  connect(aboutAction, &QAction::triggered, this, &StartupWindow::showAboutDialog);

  // Add Cmd+Q shortcut to quit the application
  auto* quitShortcut = new QShortcut(QKeySequence::Quit, this);
  connect(quitShortcut, &QShortcut::activated, qApp, &QApplication::quit);
#endif

  // Set a reasonable size
  resize(500, 400);
}

StartupWindow::~StartupWindow() = default;

void StartupWindow::closeEvent(QCloseEvent* event) {
  // When the startup window is closed directly, quit the application
  event->accept();
  QApplication::quit();
}

void StartupWindow::showAboutDialog() {
  Ui::AboutDialog ui;
  auto* dialog = new QDialog(this);
  ui.setupUi(dialog);
  ui.version->setText(QString(tr("version ")) + QString::fromUtf8(VERSION));

  QResource license(":/GPLv3.html");
  ui.licenseViewer->setHtml(QString::fromUtf8((const char*) license.data(), static_cast<int>(license.size())));

  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setWindowModality(Qt::WindowModal);
  dialog->show();
}
