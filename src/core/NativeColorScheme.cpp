// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "NativeColorScheme.h"

#include <QFile>
#include <QPalette>
#include <QStyleFactory>

#include "Utils.h"

NativeColorScheme::NativeColorScheme() {
  loadStyleSheet();
  loadColorParams();
}

QStyle* NativeColorScheme::getStyle() const {
  // Use Fusion so QSS has full control over widget rendering
  return QStyleFactory::create("Fusion");
}

const QPalette* NativeColorScheme::getPalette() const {
  return nullptr;
}

const QString* NativeColorScheme::getStyleSheet() const {
  return &m_styleSheet;
}

const ColorScheme::ColorParams* NativeColorScheme::getColorParams() const {
  return &m_customColors;
}

void NativeColorScheme::loadStyleSheet() {
  // The bundled light stylesheet hardcodes light colors (#fff, #e8e8e8, etc.)
  // and is unreadable on a dark system palette. In the "native" scheme we only
  // apply it when the OS appearance is light; dark-mode users either stay on
  // native chrome (here) or can explicitly choose the Dark scheme.
  const QPalette palette = QPalette();
  if (palette.window().color().lightnessF() < 0.5) {
    return;
  }

  QFile styleSheetFile(QString::fromUtf8(":/light_scheme/stylesheet/stylesheet.qss"));
  if (styleSheetFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    m_styleSheet = styleSheetFile.readAll();
    styleSheetFile.close();
  }

#ifdef _WIN32
  m_styleSheet = core::Utils::qssConvertPxToEm(m_styleSheet, 13, 4);
#else
  m_styleSheet = core::Utils::qssConvertPxToEm(m_styleSheet, 16, 4);
#endif
}

void NativeColorScheme::loadColorParams() {
  const QPalette palette = QPalette();

  m_customColors["ThumbnailSequenceSelectedItemBackground"] = palette.color(QPalette::Highlight).lighter(130);
  m_customColors["ThumbnailSequenceSelectionLeaderBackground"] = palette.color(QPalette::Highlight);
  m_customColors["ProcessingIndicationFade"] = palette.window().color().darker(115);
  if (palette.window().color().lightnessF() < 0.5) {
    m_customColors["ProcessingIndicationHead"] = palette.color(QPalette::Window).lighter(200);
    m_customColors["ProcessingIndicationTail"] = palette.color(QPalette::Window).lighter(130);
    m_customColors["StageListHead"] = m_customColors.at("ProcessingIndicationHead");
    m_customColors["StageListTail"] = m_customColors.at("ProcessingIndicationTail");
  }
}
