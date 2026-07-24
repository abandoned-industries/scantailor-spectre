// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "Params.h"

#include "XmlMarshaller.h"
#include "XmlUnmarshaller.h"

namespace page_box {
Params::Params(const Dependencies& deps)
    : m_deps(deps), m_pageDetectionMode(MODE_DISABLED), m_fineTuneCorners(false) {}

Params::Params(const QRectF& pageRect,
               const Dependencies& deps,
               const AutoManualMode pageDetectionMode,
               const bool fineTuneCorners)
    : m_pageRect(pageRect),
      m_deps(deps),
      m_pageDetectionMode(pageDetectionMode),
      m_fineTuneCorners(fineTuneCorners) {}

Params::Params(const QDomElement& el)
    : m_pageRect(XmlUnmarshaller::rectF(el.namedItem("page-rect").toElement())),
      m_deps(el.namedItem("dependencies").toElement()),
      m_pageDetectionMode(stringToAutoManualMode(el.attribute("pageDetectionMode"))),
      m_fineTuneCorners(el.attribute("fineTuneCorners") == "1") {}

Params::~Params() = default;

QDomElement Params::toXml(QDomDocument& doc, const QString& name) const {
  XmlMarshaller marshaller(doc);

  QDomElement el(doc.createElement(name));
  el.setAttribute("pageDetectionMode", autoManualModeToString(m_pageDetectionMode));
  el.setAttribute("fineTuneCorners", m_fineTuneCorners ? "1" : "0");
  el.appendChild(marshaller.rectF(m_pageRect, "page-rect"));
  el.appendChild(m_deps.toXml(doc, "dependencies"));
  return el;
}
}  // namespace page_box
