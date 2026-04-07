// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_PAGE_BOX_DEPENDENCIES_H_
#define SCANTAILOR_PAGE_BOX_DEPENDENCIES_H_

#include <AutoManualMode.h>

#include <QPolygonF>

class QDomDocument;
class QDomElement;
class QString;

namespace page_box {
class Dependencies {
 public:
  Dependencies() = default;

  explicit Dependencies(const QPolygonF& rotatedPageOutline);

  Dependencies(const QPolygonF& rotatedPageOutline, AutoManualMode pageDetectionMode, bool fineTuneCorners);

  explicit Dependencies(const QDomElement& depsEl);

  ~Dependencies() = default;

  const QPolygonF& rotatedPageOutline() const { return m_rotatedPageOutline; }

  bool compatibleWith(const Dependencies& other, bool* needUpdate = nullptr) const;

  QDomElement toXml(QDomDocument& doc, const QString& name) const;

 private:
  QPolygonF m_rotatedPageOutline;
  AutoManualMode m_pageDetectionMode = MODE_DISABLED;
  bool m_fineTuneCorners = false;
};
}  // namespace page_box
#endif
