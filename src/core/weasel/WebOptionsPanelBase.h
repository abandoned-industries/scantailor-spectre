// Copyright (C) 2026  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_WEASEL_WEBOPTIONSPANELBASE_H_
#define SCANTAILOR_WEASEL_WEBOPTIONSPANELBASE_H_

#include <QString>
#include <QWidget>

class QWebEngineView;
class QWebChannel;
class QVariant;

namespace weasel {

/**
 * Base class for web-based filter option panels.
 *
 * Handles the common plumbing: embedding a QWebEngineView, setting up
 * a QWebChannel, loading an HTML file from Qt resources, and exposing a
 * registerBridge() hook that subclasses call to expose their bridge object
 * to JavaScript.
 *
 * HTML files live under qrc:/weasel/webui/ and share panel.css / panel.js
 * from qrc:/weasel/webui/shared/.
 */
class WebOptionsPanelBase : public QWidget {
  Q_OBJECT
 public:
  /**
   * \param htmlPath  Path relative to qrc:/weasel/webui/ (e.g. "photo_adjustments.html")
   */
  explicit WebOptionsPanelBase(const QString& htmlPath, QWidget* parent = nullptr);

  ~WebOptionsPanelBase() override;

  /**
   * Register the bridge object that JavaScript will see as 'bridge'.
   * Must be called right after construction, before the event loop runs.
   * The bridge must be a QObject with signals/slots JS will invoke.
   */
  void registerBridge(QObject* bridge);

 protected:
  QWebEngineView* webView() const { return m_view; }
  QWebChannel* channel() const { return m_channel; }

 private:
  void updateHeightFromContent(const QVariant& height);

  QWebEngineView* m_view;
  QWebChannel* m_channel;
};

}  // namespace weasel

#endif
