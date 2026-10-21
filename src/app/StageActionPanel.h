// Copyright (C) 2026  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_APP_STAGEACTIONPANEL_H_
#define SCANTAILOR_APP_STAGEACTIONPANEL_H_

#include <QWidget>

class QLabel;
class QGridLayout;
class QPushButton;
class QResizeEvent;
class QShowEvent;
class QToolButton;
class QVBoxLayout;

class StageActionPanel : public QWidget {
  Q_OBJECT

 public:
  explicit StageActionPanel(QWidget* controls, QWidget* parent = nullptr);

  void setPrimaryEnabled(bool enabled);
  void setHeadingText(const QString& text);
  void setActionTexts(const QString& primary, const QString& secondary);
  void setReviewMode(bool reviewMode);
  void setActionRowVisible(bool visible);
  void setAdjustWidget(QWidget* widget);
  void setPinnedWidget(QWidget* widget);
  void setScopeText(const QString& text);
  void setWaitingText(const QString& text);
  void setAdjustOpen(bool open);
  bool adjustOpen() const;
  int minimumContentWidth() const;

 protected:
  void resizeEvent(QResizeEvent* event) override;
  void showEvent(QShowEvent* event) override;

 signals:
  void processAllRequested();
  void thisPageRequested();
  void reviewPrimaryRequested();
  void reviewSecondaryRequested();
  void adjustToggled(bool open);

 private:
  void updateActionLayout();

  QLabel* m_heading;
  QWidget* m_actionRow;
  QGridLayout* m_actionLayout;
  QPushButton* m_primary;
  QPushButton* m_secondary;
  QToolButton* m_adjustToggle;
  QWidget* m_foldBody;
  QVBoxLayout* m_adjustLayout;
  QWidget* m_adjustWidget = nullptr;
  QWidget* m_pinnedSlot;
  QVBoxLayout* m_pinnedLayout;
  QWidget* m_pinnedWidget = nullptr;
  QLabel* m_scopeLabel;
  QLabel* m_waitingLabel;
  bool m_actionsStacked = false;
  bool m_reviewMode = false;
};

#endif  // SCANTAILOR_APP_STAGEACTIONPANEL_H_
