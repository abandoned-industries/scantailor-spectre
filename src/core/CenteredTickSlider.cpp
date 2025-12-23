// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "CenteredTickSlider.h"

#include <QPainter>
#include <QStyleOptionSlider>

CenteredTickSlider::CenteredTickSlider(QWidget* parent) : QSlider(Qt::Horizontal, parent) {
  setTickPosition(QSlider::TicksAbove);
}

void CenteredTickSlider::paintEvent(QPaintEvent* event) {
  QSlider::paintEvent(event);

  QPainter painter(this);
  drawTickMarks(painter);
}

void CenteredTickSlider::drawTickMarks(QPainter& painter) {
  QStyleOptionSlider opt;
  initStyleOption(&opt);

  const QRect grooveRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
  const int grooveTop = grooveRect.top();

  const int minVal = minimum();
  const int maxVal = maximum();
  const int range = maxVal - minVal;
  if (range == 0)
    return;

  // Calculate positions for tick marks
  const int sliderMin = grooveRect.left() + 6;   // Account for handle width
  const int sliderMax = grooveRect.right() - 6;
  const int sliderRange = sliderMax - sliderMin;

  painter.setRenderHint(QPainter::Antialiasing, false);

  // Draw tick marks at -100, -50, 0, 50, 100 (assuming range -100 to 100)
  QVector<int> tickValues = {minVal, minVal + range / 4, (minVal + maxVal) / 2, maxVal - range / 4, maxVal};

  for (int tickVal : tickValues) {
    const double ratio = static_cast<double>(tickVal - minVal) / range;
    const int x = sliderMin + static_cast<int>(ratio * sliderRange);

    const bool isCenter = (tickVal == (minVal + maxVal) / 2);

    if (isCenter) {
      // Draw center tick mark - taller and darker (above groove)
      painter.setPen(QPen(QColor(80, 80, 80), 2));
      painter.drawLine(x, grooveTop - 4, x, grooveTop - 12);
    } else {
      // Draw regular tick mark (above groove)
      painter.setPen(QPen(QColor(150, 150, 150), 1));
      painter.drawLine(x, grooveTop - 4, x, grooveTop - 9);
    }
  }
}
