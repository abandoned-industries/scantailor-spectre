// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "ProjectFolder.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

ProjectFolder::ProjectFolder(const QString& folderPath) : m_basePath(folderPath) {}

QString ProjectFolder::projectFilePath() const {
  return QDir(m_basePath).filePath("project.ScanTailor");
}

QString ProjectFolder::originalsDir() const {
  return QDir(m_basePath).filePath("originals");
}

QString ProjectFolder::cacheDir() const {
  return QDir(m_basePath).filePath("cache");
}

QString ProjectFolder::outputDir() const {
  return QDir(m_basePath).filePath("output");
}

bool ProjectFolder::create() {
  QDir dir(m_basePath);

  // Create base directory if it doesn't exist
  if (!dir.exists() && !dir.mkpath(".")) {
    return false;
  }

  // Create subdirectories
  bool success = true;
  success = success && (dir.exists("originals") || dir.mkdir("originals"));
  success = success && (dir.exists("cache") || dir.mkdir("cache"));
  success = success && (dir.exists("output") || dir.mkdir("output"));

  return success;
}

QString ProjectFolder::copyOriginal(const QString& sourcePath) {
  QFileInfo sourceInfo(sourcePath);
  if (!sourceInfo.exists()) {
    return QString();
  }

  QString destPath = QDir(originalsDir()).filePath(sourceInfo.fileName());

  // Handle filename conflicts by adding numeric suffix
  if (QFile::exists(destPath)) {
    const QString baseName = sourceInfo.completeBaseName();
    const QString suffix = sourceInfo.suffix();
    int counter = 1;

    while (QFile::exists(destPath)) {
      QString newName;
      if (suffix.isEmpty()) {
        newName = QString("%1_%2").arg(baseName).arg(counter++);
      } else {
        newName = QString("%1_%2.%3").arg(baseName).arg(counter++).arg(suffix);
      }
      destPath = QDir(originalsDir()).filePath(newName);
    }
  }

  if (QFile::copy(sourcePath, destPath)) {
    return destPath;
  }

  return QString();
}

bool ProjectFolder::isValidProjectFolder(const QString& path) {
  QDir dir(path);
  return dir.exists() && dir.exists("project.ScanTailor");
}
