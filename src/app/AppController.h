// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_APP_APPCONTROLLER_H_
#define SCANTAILOR_APP_APPCONTROLLER_H_

#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QPointer>
#include <QString>

class StartupWindow;
class MainWindow;
class ProjectCreationContext;

class AppController : public QObject {
  Q_OBJECT

 public:
  explicit AppController(QObject* parent = nullptr);
  ~AppController() override;

  void start();
  void openProject(const QString& path);

 public slots:
  void showStartupWindow();

 private slots:
  void onOpenProjectRequested();
  void onImportPdfRequested();
  void onImportFolderRequested();
  void onRecentProjectRequested(const QString& path);
  void onMainWindowProjectClosed();
  void onNewProjectFromMainWindow();
  void onProjectCreationDone(ProjectCreationContext* context);

 private:
  void showMainWindow();
  void connectStartupWindow();
  void connectMainWindow();

  QPointer<StartupWindow> m_startupWindow;
  QPointer<MainWindow> m_mainWindow;
};

#endif  // SCANTAILOR_APP_APPCONTROLLER_H_
