// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_PAGE_BOX_PARAMS_H_
#define SCANTAILOR_PAGE_BOX_PARAMS_H_

#include <AutoManualMode.h>

#include <QRectF>

#include "Dependencies.h"

class QDomDocument;
class QDomElement;
class QString;

namespace page_box {
class Params {
 public:
  Params() : m_pageDetectionMode(MODE_DISABLED), m_fineTuneCorners(false) {}

  explicit Params(const Dependencies& deps);

  Params(const QRectF& pageRect, const Dependencies& deps, AutoManualMode pageDetectionMode, bool fineTuneCorners);

  explicit Params(const QDomElement& el);

  QDomElement toXml(QDomDocument& doc, const QString& name) const;

  ~Params();

  const QRectF& pageRect() const { return m_pageRect; }
  void setPageRect(const QRectF& rect) { m_pageRect = rect; }

  const Dependencies& dependencies() const { return m_deps; }
  void setDependencies(const Dependencies& deps) { m_deps = deps; }

  AutoManualMode pageDetectionMode() const { return m_pageDetectionMode; }
  void setPageDetectionMode(AutoManualMode mode) { m_pageDetectionMode = mode; }

  bool isFineTuningEnabled() const { return m_fineTuneCorners; }
  void setFineTuneCornersEnabled(bool v) { m_fineTuneCorners = v; }

 private:
  QRectF m_pageRect;
  Dependencies m_deps;
  AutoManualMode m_pageDetectionMode;
  bool m_fineTuneCorners;
};
}  // namespace page_box
#endif
