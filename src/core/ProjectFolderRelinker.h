// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_CORE_PROJECTFOLDERRELINKER_H_
#define SCANTAILOR_CORE_PROJECTFOLDERRELINKER_H_

#include <QMap>
#include <QString>

#include "AbstractRelinker.h"

/**
 * \brief Relinker that maps original file paths to new paths in a project folder.
 *
 * Used when saving a project to a self-contained folder to update all
 * image references to point to the copied files in originals/.
 */
class ProjectFolderRelinker : public AbstractRelinker {
 public:
  ProjectFolderRelinker() = default;

  /**
   * Adds a path mapping from original to new location.
   */
  void addMapping(const QString& from, const QString& to);

  /**
   * Returns the substitution path for the given path.
   * If no mapping exists, returns the original path unchanged.
   */
  QString substitutionPathFor(const RelinkablePath& path) const override;

 private:
  QMap<QString, QString> m_mappings;
};

#endif  // SCANTAILOR_CORE_PROJECTFOLDERRELINKER_H_
