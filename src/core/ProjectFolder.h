// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_CORE_PROJECTFOLDER_H_
#define SCANTAILOR_CORE_PROJECTFOLDER_H_

#include <QString>

/**
 * \brief Manages a self-contained project folder structure.
 *
 * Project folder layout:
 *   ProjectFolder/
 *   ├── project.ScanTailor   (XML project file)
 *   ├── originals/           (copies of input images)
 *   ├── cache/               (thumbnails, processing cache)
 *   └── output/              (processed output files)
 */
class ProjectFolder {
 public:
  explicit ProjectFolder(const QString& folderPath);

  QString basePath() const { return m_basePath; }
  QString projectFilePath() const;
  QString originalsDir() const;
  QString cacheDir() const;
  QString outputDir() const;

  /**
   * Creates the folder structure (base folder and subdirectories).
   * \return true on success, false on failure.
   */
  bool create();

  /**
   * Copies a source file to the originals/ directory.
   * Handles filename conflicts by adding numeric suffixes.
   * \param sourcePath The path to the source file.
   * \return The new path in originals/, or empty string on failure.
   */
  QString copyOriginal(const QString& sourcePath);

  /**
   * Checks if a folder appears to be a valid project folder.
   */
  static bool isValidProjectFolder(const QString& path);

 private:
  QString m_basePath;
};

#endif  // SCANTAILOR_CORE_PROJECTFOLDER_H_
