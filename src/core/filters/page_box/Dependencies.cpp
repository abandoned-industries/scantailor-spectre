// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "Dependencies.h"

#include <PolygonUtils.h>

#include "XmlMarshaller.h"
#include "XmlUnmarshaller.h"

using namespace imageproc;

namespace page_box {
Dependencies::Dependencies(const QPolygonF& rotatedPageOutline)
    : m_rotatedPageOutline(rotatedPageOutline) {}

Dependencies::Dependencies(const QPolygonF& rotatedPageOutline,
                           const AutoManualMode pageDetectionMode,
                           const bool fineTuneCorners)
    : m_rotatedPageOutline(rotatedPageOutline),
      m_pageDetectionMode(pageDetectionMode),
      m_fineTuneCorners(fineTuneCorners) {}

Dependencies::Dependencies(const QDomElement& depsEl)
    : m_rotatedPageOutline(XmlUnmarshaller::polygonF(depsEl.namedItem("rotated-page-outline").toElement())),
      m_pageDetectionMode(stringToAutoManualMode(depsEl.attribute("pageDetectionMode"))),
      m_fineTuneCorners(depsEl.attribute("fineTuneCorners") == "1") {}

bool Dependencies::compatibleWith(const Dependencies& other, bool* needUpdate) const {
  bool need = false;

  if (!PolygonUtils::fuzzyCompare(m_rotatedPageOutline, other.m_rotatedPageOutline)) {
    need = true;
  } else if ((m_pageDetectionMode != MODE_MANUAL) && (m_pageDetectionMode != other.m_pageDetectionMode)) {
    need = true;
  } else if ((m_pageDetectionMode == MODE_AUTO) && (m_fineTuneCorners != other.m_fineTuneCorners)) {
    need = true;
  }

  if (needUpdate) {
    *needUpdate = need;
  }
  return !need;
}

QDomElement Dependencies::toXml(QDomDocument& doc, const QString& name) const {
  XmlMarshaller marshaller(doc);

  QDomElement el(doc.createElement(name));
  el.appendChild(marshaller.polygonF(m_rotatedPageOutline, "rotated-page-outline"));
  el.setAttribute("pageDetectionMode", autoManualModeToString(m_pageDetectionMode));
  el.setAttribute("fineTuneCorners", m_fineTuneCorners ? "1" : "0");
  return el;
}
}  // namespace page_box
