// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "CenteredTickSlider.h"

#include <QPainter>
#include <QProxyStyle>
#include <QStyleOptionSlider>

namespace {
class JumpClickStyle : public QProxyStyle {
 public:
  using QProxyStyle::QProxyStyle;
  int styleHint(StyleHint hint, const QStyleOption* option, const QWidget* widget,
                QStyleHintReturn* returnData) const override {
    if (hint == QStyle::SH_Slider_AbsoluteSetButtons) {
      return Qt::LeftButton;
    }
    return QProxyStyle::styleHint(hint, option, widget, returnData);
  }
};
}  // namespace

CenteredTickSlider::CenteredTickSlider(QWidget* parent) : QSlider(Qt::Horizontal, parent) {
  setTickPosition(QSlider::NoTicks);
  auto* jumpStyle = new JumpClickStyle(style());
  jumpStyle->setParent(this);
  setStyle(jumpStyle);
}

void CenteredTickSlider::paintEvent(QPaintEvent* event) {
  QSlider::paintEvent(event);
}

void CenteredTickSlider::drawTickMarks(QPainter& painter) {
}
