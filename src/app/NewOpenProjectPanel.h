// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_APP_NEWOPENPROJECTPANEL_H_
#define SCANTAILOR_APP_NEWOPENPROJECTPANEL_H_

#include <QWidget>

class QLabel;
class QString;
class QVBoxLayout;

class NewOpenProjectPanel : public QWidget {
  Q_OBJECT
 public:
  explicit NewOpenProjectPanel(QWidget* parent = nullptr);

 signals:
  void importPdf();
  void importFolder();
  void openProject();
  void openRecentProject(const QString& projectFile);

 private:
  void addRecentProject(const QString& filePath);

  QWidget* m_recentContainer = nullptr;
  QVBoxLayout* m_recentLayout = nullptr;
  QLabel* m_noRecentLabel = nullptr;
};

#endif  // ifndef SCANTAILOR_APP_NEWOPENPROJECTPANEL_H_
