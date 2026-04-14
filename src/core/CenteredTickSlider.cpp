// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "CenteredTickSlider.h"

#include <QPainter>
#include <QStyleOptionSlider>

CenteredTickSlider::CenteredTickSlider(QWidget* parent) : QSlider(Qt::Horizontal, parent) {
  setTickPosition(QSlider::NoTicks);
}

void CenteredTickSlider::paintEvent(QPaintEvent* event) {
  QSlider::paintEvent(event);
  // No tick marks drawn — clean slider
}

void CenteredTickSlider::drawTickMarks(QPainter& painter) {
}
