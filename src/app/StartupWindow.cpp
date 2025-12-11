// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "StartupWindow.h"

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

  // Set a reasonable size
  resize(500, 400);
}

StartupWindow::~StartupWindow() = default;
