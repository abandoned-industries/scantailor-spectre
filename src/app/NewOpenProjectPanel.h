// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_APP_NEWOPENPROJECTPANEL_H_
#define SCANTAILOR_APP_NEWOPENPROJECTPANEL_H_

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

class QString;

// Clickable label with hover color change
class ClickableLabel : public QLabel {
  Q_OBJECT
 public:
  explicit ClickableLabel(const QString& text, QWidget* parent = nullptr);
  void setTextColor(const QColor& color);

 signals:
  void clicked();

 protected:
  void enterEvent(QEnterEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;

 private:
  void applyColor(const QColor& color);
  QColor m_normalColor;
  QColor m_hoverColor;
};

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

  QWidget* m_recentContainer;
  QVBoxLayout* m_recentLayout;
  QLabel* m_noRecentLabel;
};

#endif  // ifndef SCANTAILOR_APP_NEWOPENPROJECTPANEL_H_
