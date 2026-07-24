// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "Settings.h"

#include "AbstractRelinker.h"
#include "RelinkablePath.h"

namespace page_box {
Settings::Settings() : m_pageDetectionTolerance(0.1) {}

Settings::~Settings() = default;

void Settings::clear() {
  QMutexLocker locker(&m_mutex);
  m_pageParams.clear();
  m_deviationProvider.clear();
}

void Settings::performRelinking(const AbstractRelinker& relinker) {
  QMutexLocker locker(&m_mutex);
  PageParams newParams;
  for (auto& [pageId, params] : m_pageParams) {
    const RelinkablePath oldPath(pageId.imageId().filePath(), RelinkablePath::File);
    PageId newPageId(pageId);
    newPageId.imageId().setFilePath(relinker.substitutionPathFor(oldPath));
    newParams[newPageId] = params;
  }
  m_pageParams.swap(newParams);
}

void Settings::setPageParams(const PageId& pageId, const Params& params) {
  QMutexLocker locker(&m_mutex);
  m_pageParams[pageId] = params;

  // Track page rect width for deviation detection
  const QRectF& pageRect = params.pageRect();
  if (pageRect.isValid()) {
    m_deviationProvider.addOrUpdate(pageId, pageRect.width());
  }
}

void Settings::clearPageParams(const PageId& pageId) {
  QMutexLocker locker(&m_mutex);
  m_pageParams.erase(pageId);
  m_deviationProvider.remove(pageId);
}

std::unique_ptr<Params> Settings::getPageParams(const PageId& pageId) const {
  QMutexLocker locker(&m_mutex);
  auto it = m_pageParams.find(pageId);
  if (it != m_pageParams.end()) {
    return std::make_unique<Params>(it->second);
  }
  return nullptr;
}

bool Settings::isParamsNull(const PageId& pageId) const {
  QMutexLocker locker(&m_mutex);
  return m_pageParams.find(pageId) == m_pageParams.end();
}

QSizeF Settings::pageDetectionBox() const {
  QMutexLocker locker(&m_mutex);
  return m_pageDetectionBox;
}

void Settings::setPageDetectionBox(QSizeF size) {
  QMutexLocker locker(&m_mutex);
  m_pageDetectionBox = size;
}

double Settings::pageDetectionTolerance() const {
  QMutexLocker locker(&m_mutex);
  return m_pageDetectionTolerance;
}

void Settings::setPageDetectionTolerance(double tolerance) {
  QMutexLocker locker(&m_mutex);
  m_pageDetectionTolerance = tolerance;
}

const DeviationProvider<PageId>& Settings::deviationProvider() const {
  return m_deviationProvider;
}
}  // namespace page_box
