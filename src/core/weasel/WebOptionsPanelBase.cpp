// Copyright (C) 2026  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "WebOptionsPanelBase.h"

#include <QColor>
#include <QFile>
#include <QPointer>
#include <QUrl>
#include <QVariant>
#include <QVBoxLayout>
#include <QWebChannel>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QWebEngineView>

// Force linking of the webui.qrc resource when compiled into a static library.
// Q_INIT_RESOURCE cannot be called from an anonymous namespace.
static void initWebUIResources() {
  static bool initialized = false;
  if (!initialized) {
    Q_INIT_RESOURCE(webui);
    initialized = true;
  }
}

namespace weasel {

WebOptionsPanelBase::WebOptionsPanelBase(const QString& htmlPath, QWidget* parent)
    : QWidget(parent),
      m_view(new QWebEngineView(this)),
      m_channel(new QWebChannel(this)) {
  initWebUIResources();

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(m_view);

  m_view->page()->setWebChannel(m_channel);
  // Expose this panel itself on the channel so panel.js can ping remeasure()
  // from a ResizeObserver when DOM sections hide/show.
  m_channel->registerObject(QStringLiteral("panelBase"), this);

  // NOTE: Do NOT use Qt::transparent here. On macOS + Qt 6.x, transparent
  // background on QWebEnginePage breaks the GPU compositor and causes the
  // view to render as a solid black rectangle. Match the HTML body background.
  m_view->page()->setBackgroundColor(Qt::white);

  auto* settings = m_view->settings();
  settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
  settings->setAttribute(QWebEngineSettings::ShowScrollBars, false);

  // Read HTML content directly from Qt resources, then setHtml() with a base
  // URL so relative CSS/JS paths in the document resolve correctly.
  const QString resourcePath = QStringLiteral(":/weasel/webui/") + htmlPath;
  QFile file(resourcePath);
  if (file.open(QIODevice::ReadOnly)) {
    const QString html = QString::fromUtf8(file.readAll());
    m_view->setHtml(html, QUrl("qrc:/weasel/webui/"));
  }

  connect(m_view->page(), &QWebEnginePage::loadFinished, this, [this](bool ok) {
    emit loadFinished(ok);
    if (!ok) {
      return;
    }
    // runJavaScript's callback fires asynchronously after the JS engine
    // returns — if the panel has been destroyed in the meantime, `this`
    // dangles. Guard with QPointer so the callback becomes a no-op instead
    // of a use-after-free.
    QPointer<WebOptionsPanelBase> self(this);
    m_view->page()->runJavaScript(
        QStringLiteral("Math.ceil(Math.max(document.body.scrollHeight, document.documentElement.scrollHeight));"),
        [self](const QVariant& height) {
          if (!self) {
            return;
          }
          self->updateHeightFromContent(height);
        });
  });
}

WebOptionsPanelBase::~WebOptionsPanelBase() = default;

void WebOptionsPanelBase::remeasure() {
  if (!m_view || !m_view->page()) {
    return;
  }
  QPointer<WebOptionsPanelBase> self(this);
  m_view->page()->runJavaScript(
      QStringLiteral("Math.ceil(Math.max(document.body.scrollHeight, document.documentElement.scrollHeight));"),
      [self](const QVariant& height) {
        if (!self) {
          return;
        }
        self->updateHeightFromContent(height);
      });
}

void WebOptionsPanelBase::registerBridge(QObject* bridge) {
  m_channel->registerObject(QStringLiteral("bridge"), bridge);
}

void WebOptionsPanelBase::updateHeightFromContent(const QVariant& height) {
  bool ok = false;
  const int contentHeight = height.toInt(&ok);
  if (!ok || contentHeight <= 0) {
    return;
  }

  const int panelHeight = contentHeight + 4;
  m_view->setMinimumHeight(panelHeight);
  m_view->setMaximumHeight(panelHeight);
  setMinimumHeight(panelHeight);
  setMaximumHeight(panelHeight);
  updateGeometry();
}

}  // namespace weasel
