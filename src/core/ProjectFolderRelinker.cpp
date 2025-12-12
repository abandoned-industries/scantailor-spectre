// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "ProjectFolderRelinker.h"

#include "RelinkablePath.h"

void ProjectFolderRelinker::addMapping(const QString& from, const QString& to) {
  m_mappings[RelinkablePath::normalize(from)] = to;
}

QString ProjectFolderRelinker::substitutionPathFor(const RelinkablePath& path) const {
  const QString& normalizedPath = path.normalizedPath();
  auto it = m_mappings.find(normalizedPath);
  if (it != m_mappings.end()) {
    return it.value();
  }
  return normalizedPath;
}
