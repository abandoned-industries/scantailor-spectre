// Copyright (C) 2026 ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include <ProjectReader.h>
#include <ProjectWriter.h>

#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QTemporaryDir>
#include <boost/test/unit_test.hpp>
#include <memory>
#include <vector>

#include "Dpi.h"
#include "FileNameDisambiguator.h"
#include "ImageFileInfo.h"
#include "ImageId.h"
#include "ImageInfo.h"
#include "ImageMetadata.h"
#include "OutputFileNameGenerator.h"
#include "ProjectPages.h"
#include "SelectedPage.h"

namespace Tests {
namespace {
std::shared_ptr<ProjectPages> makePages(const QString& imagePath) {
  const ImageId imageId(imagePath, 0);
  const ImageMetadata metadata(QSize(100, 200), Dpi(300, 300));
  const std::vector<ImageInfo> images{ImageInfo(imageId, metadata, 1, false, false)};
  return std::make_shared<ProjectPages>(images, Qt::LeftToRight);
}

bool writeProject(const QString& projectFile, const QString& imagePath, const QString& outDir) {
  auto pages = makePages(imagePath);
  const OutputFileNameGenerator gen(std::make_shared<FileNameDisambiguator>(), outDir, Qt::LeftToRight);
  const ProjectWriter writer(pages, SelectedPage(), gen);
  return writer.write(projectFile, {});
}

QDomDocument loadDoc(const QString& projectFile) {
  QFile file(projectFile);
  BOOST_REQUIRE(file.open(QIODevice::ReadOnly));
  QDomDocument doc;
  BOOST_REQUIRE(doc.setContent(&file));
  return doc;
}
}  // namespace

BOOST_AUTO_TEST_SUITE(ProjectPortabilityTestSuite)

// Paths under the project directory are written relative to the project file.
BOOST_AUTO_TEST_CASE(same_volume_paths_written_relative) {
  QTemporaryDir temp;
  BOOST_REQUIRE(temp.isValid());
  const QDir projectDir(temp.path());
  BOOST_REQUIRE(projectDir.mkpath("scans"));
  BOOST_REQUIRE(projectDir.mkpath("out"));

  const QString projectFile = projectDir.filePath("project.ScanTailor");
  BOOST_REQUIRE(writeProject(projectFile, projectDir.filePath("scans/page1.tif"), projectDir.filePath("out")));

  const QDomDocument doc = loadDoc(projectFile);
  const QDomElement rootEl = doc.documentElement();
  BOOST_CHECK(rootEl.attribute("outputDirectory") == "out");
  const QDomElement dirEl = rootEl.namedItem("directories").firstChildElement("directory");
  BOOST_CHECK(dirEl.attribute("path") == "scans");
}

// A saved project reopens correctly after its folder is moved: relative paths
// resolve against the project file's new location.
BOOST_AUTO_TEST_CASE(moved_project_folder_resolves_paths) {
  QTemporaryDir temp;
  BOOST_REQUIRE(temp.isValid());
  const QDir base(temp.path());
  BOOST_REQUIRE(base.mkpath("original/scans"));

  const QString projectFile = base.filePath("original/project.ScanTailor");
  BOOST_REQUIRE(
      writeProject(projectFile, base.filePath("original/scans/page1.tif"), base.filePath("original/out")));

  // "Move" the project folder.
  BOOST_REQUIRE(QDir().rename(base.filePath("original"), base.filePath("moved")));
  const QString movedFile = base.filePath("moved/project.ScanTailor");

  const ProjectReader reader(loadDoc(movedFile), movedFile);
  BOOST_REQUIRE(reader.success());
  BOOST_CHECK(reader.outputDirectory() == base.filePath("moved/out"));

  const std::vector<ImageFileInfo> files = reader.pages()->toImageFileInfo();
  BOOST_REQUIRE(files.size() == 1u);
  BOOST_CHECK(files.front().fileInfo().absoluteFilePath() == base.filePath("moved/scans/page1.tif"));
}

// Pre-existing projects with absolute paths keep opening exactly as before.
BOOST_AUTO_TEST_CASE(absolute_paths_read_unchanged) {
  QTemporaryDir temp;
  BOOST_REQUIRE(temp.isValid());
  const QDir base(temp.path());
  const QString absScans = base.filePath("elsewhere/scans");
  const QString absOut = base.filePath("elsewhere/out");
  BOOST_REQUIRE(QDir().mkpath(absScans));

  // Write with images outside the project dir but on the same volume: they are
  // stored relative ("../elsewhere/...") and must still resolve. Then verify a
  // hand-absolute project is left untouched.
  BOOST_REQUIRE(base.mkpath("proj"));
  const QString projectFile = base.filePath("proj/project.ScanTailor");
  BOOST_REQUIRE(writeProject(projectFile, absScans + "/page1.tif", absOut));

  const ProjectReader reader(loadDoc(projectFile), projectFile);
  BOOST_REQUIRE(reader.success());
  BOOST_CHECK(reader.outputDirectory() == absOut);

  // Now a legacy project with absolute paths, opened with a project file path.
  QDomDocument doc = loadDoc(projectFile);
  QDomElement rootEl = doc.documentElement();
  rootEl.setAttribute("outputDirectory", absOut);
  QDomElement dirEl = rootEl.namedItem("directories").firstChildElement("directory");
  dirEl.setAttribute("path", absScans);

  const ProjectReader legacyReader(doc, projectFile);
  BOOST_REQUIRE(legacyReader.success());
  BOOST_CHECK(legacyReader.outputDirectory() == absOut);
  const std::vector<ImageFileInfo> files = legacyReader.pages()->toImageFileInfo();
  BOOST_REQUIRE(files.size() == 1u);
  BOOST_CHECK(files.front().fileInfo().absoluteFilePath() == absScans + "/page1.tif");
}

BOOST_AUTO_TEST_SUITE_END()
}  // namespace Tests
