// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "StartupWindow.h"

#include <QApplication>
#include <QShortcut>
#include <QVBoxLayout>

#include "NewOpenProjectPanel.h"

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
