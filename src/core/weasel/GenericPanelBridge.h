// Copyright (C) 2026  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_WEASEL_GENERICPANELBRIDGE_H_
#define SCANTAILOR_WEASEL_GENERICPANELBRIDGE_H_

#include <QObject>
#include <QString>
#include <QVariantMap>

namespace weasel {

/**
 * Generic bridge between JavaScript panels and C++ OptionsWidget classes.
 *
 * JavaScript calls its public slots (setValue, setCheck, setRadio, setSelect, action),
 * which are re-emitted as typed signals that C++ code connects to.
 *
 * C++ pushes panel state to JS by calling setState() with a map of values; this emits
 * valuesChanged() which JS connects to and applies via the Panel framework in panel.js.
 *
 * This avoids having to write a custom QObject bridge for every filter.
 */
class GenericPanelBridge : public QObject {
  Q_OBJECT
 public:
  explicit GenericPanelBridge(QObject* parent = nullptr);

  /// Push state to JavaScript. Keys should match element IDs in the HTML.
  void setState(const QVariantMap& values);

  const QVariantMap& state() const { return m_state; }

 signals:
  /// Emitted when setState() is called — the JS-side Panel framework receives this.
  void valuesChanged(const QVariantMap& values);

  /// Emitted when JS calls setValue (slider input). C++ handles the filter-specific logic.
  void valueChanged(const QString& id, double value);

  /// Emitted when JS calls setCheck (checkbox change).
  void checkChanged(const QString& id, bool checked);

  /// Emitted when JS calls setRadio (radio button selection).
  void radioChanged(const QString& name, const QString& selectedId);

  /// Emitted when JS calls setSelect (dropdown change).
  void selectChanged(const QString& id, const QString& value);

  /// Emitted when JS calls action (button click, menu item, etc.).
  void actionTriggered(const QString& name);

 public slots:
  // These slots are invoked from JavaScript via the QWebChannel.
  void setValue(const QString& id, double value) { emit valueChanged(id, value); }
  void setCheck(const QString& id, bool checked) { emit checkChanged(id, checked); }
  void setRadio(const QString& name, const QString& id) { emit radioChanged(name, id); }
  void setSelect(const QString& id, const QString& value) { emit selectChanged(id, value); }
  void action(const QString& name) { emit actionTriggered(name); }
  void requestValues() { emit valuesChanged(m_state); }

 private:
  QVariantMap m_state;
};

}  // namespace weasel

#endif
