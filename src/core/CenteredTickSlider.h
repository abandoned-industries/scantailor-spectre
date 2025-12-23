// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_CORE_CENTEREDTICKSLIDER_H_
#define SCANTAILOR_CORE_CENTEREDTICKSLIDER_H_

#include <QSlider>

/**
 * A horizontal slider with visible tick marks and a prominent center (zero) indicator.
 * Used for brightness/contrast adjustments where returning to neutral is common.
 */
class CenteredTickSlider : public QSlider {
  Q_OBJECT

 public:
  explicit CenteredTickSlider(QWidget* parent = nullptr);

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  void drawTickMarks(QPainter& painter);
};

#endif  // SCANTAILOR_CORE_CENTEREDTICKSLIDER_H_
