// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "AppController.h"

#include <QFileDialog>
#include <QSettings>
#include <QTimer>

#include "MainWindow.h"
#include "ProjectCreationContext.h"
#include "StartupWindow.h"

AppController::AppController(QObject* parent) : QObject(parent) {}

AppController::~AppController() = default;

void AppController::start() {
  showStartupWindow();
}

void AppController::openProject(const QString& path) {
  showMainWindow();
  if (m_mainWindow) {
    m_mainWindow->openProject(path);
  }
}

void AppController::showStartupWindow() {
  if (!m_startupWindow) {
    m_startupWindow = new StartupWindow();
    connectStartupWindow();
  }

  // Center on screen
  m_startupWindow->show();
  m_startupWindow->raise();
  m_startupWindow->activateWindow();
}

void AppController::showMainWindow() {
  // Hide startup window
  if (m_startupWindow) {
    m_startupWindow->hide();
  }

  if (!m_mainWindow) {
    m_mainWindow = new MainWindow();
    m_mainWindow->setAttribute(Qt::WA_DeleteOnClose, false);  // Don't delete, we manage it
    connectMainWindow();
  }

  QSettings settings;
  if (settings.value("mainWindow/maximized", false).toBool()) {
    QTimer::singleShot(0, m_mainWindow, &QMainWindow::showMaximized);
  } else {
    m_mainWindow->show();
  }
  m_mainWindow->raise();
  m_mainWindow->activateWindow();
}

void AppController::connectStartupWindow() {
  connect(m_startupWindow, &StartupWindow::openProjectRequested, this, &AppController::onOpenProjectRequested);
  connect(m_startupWindow, &StartupWindow::importPdfRequested, this, &AppController::onImportPdfRequested);
  connect(m_startupWindow, &StartupWindow::importFolderRequested, this, &AppController::onImportFolderRequested);
  connect(m_startupWindow, &StartupWindow::recentProjectRequested, this, &AppController::onRecentProjectRequested);
}

void AppController::connectMainWindow() {
  connect(m_mainWindow, &MainWindow::projectClosed, this, &AppController::onMainWindowProjectClosed);
  connect(m_mainWindow, &MainWindow::newProjectRequested, this, &AppController::onNewProjectFromMainWindow);
}

void AppController::onOpenProjectRequested() {
  // Show file dialog with StartupWindow as parent (so it stays in front)
  const QString projectDir(QSettings().value("project/lastDir").toString());
  const QString projectFile(QFileDialog::getOpenFileName(
      m_startupWindow, tr("Open Project"), projectDir, tr("Scan Tailor Projects") + " (*.ScanTailor)"));

  if (projectFile.isEmpty()) {
    // User cancelled - stay on startup window
    return;
  }

  // User selected a file - now show MainWindow and open the project
  showMainWindow();
  if (m_mainWindow) {
    m_mainWindow->openProject(projectFile);
  }
}

void AppController::onImportPdfRequested() {
  // Show PDF file dialog with StartupWindow as parent
  QSettings settings;
  QString lastDir = settings.value("lastInputDir").toString();
  if (lastDir.isEmpty() || !QDir(lastDir).exists()) {
    lastDir = QDir::homePath();
  }

  const QString pdfFile(
      QFileDialog::getOpenFileName(m_startupWindow, tr("Import PDF"), lastDir, tr("PDF Files") + " (*.pdf *.PDF)"));

  if (pdfFile.isEmpty()) {
    // User cancelled
    return;
  }

  // Save directory for next time
  settings.setValue("lastInputDir", QFileInfo(pdfFile).absolutePath());

  // Show MainWindow and import the PDF
  showMainWindow();
  if (m_mainWindow) {
    m_mainWindow->importPdfFile(pdfFile);
  }
}

void AppController::onImportFolderRequested() {
  // Use ProjectCreationContext to handle the dialog flow (including fix DPI if needed)
  // It will delete itself when done
  auto* context = new ProjectCreationContext(m_startupWindow);
  connect(context, &ProjectCreationContext::done, this, &AppController::onProjectCreationDone);
}

void AppController::onProjectCreationDone(ProjectCreationContext* context) {
  if (context->files().empty()) {
    // User cancelled or no files selected
    return;
  }

  // Show MainWindow and create the project
  showMainWindow();
  if (m_mainWindow) {
    m_mainWindow->createProjectFromFiles(QString(),  // inputDir not needed
                                         context->outDir(), context->files(),
                                         context->layoutDirection() == Qt::RightToLeft, false);
  }
}

void AppController::onRecentProjectRequested(const QString& path) {
  showMainWindow();
  if (m_mainWindow) {
    m_mainWindow->openProject(path);
  }
}

void AppController::onMainWindowProjectClosed() {
  if (m_mainWindow) {
    m_mainWindow->hide();
  }
  showStartupWindow();
}

void AppController::onNewProjectFromMainWindow() {
  // User clicked "New Project" from MainWindow - show startup window
  if (m_mainWindow) {
    m_mainWindow->hide();
  }
  showStartupWindow();
}
