// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_CORE_OUTPUTFILENAMEGENERATOR_H_
#define SCANTAILOR_CORE_OUTPUTFILENAMEGENERATOR_H_

#include <QString>
#include <Qt>
#include <memory>

#include "FileNameDisambiguator.h"

class PageId;
class AbstractRelinker;

// Output format enum for file naming
enum class OutputImageFormat {
  TIFF,
  PNG,
  JPEG
};

// TIFF compression options
enum class OutputTiffCompression {
  LZW,
  Deflate
};

class OutputFileNameGenerator {
  // Member-wise copying is OK.
 public:
  OutputFileNameGenerator();

  OutputFileNameGenerator(std::shared_ptr<FileNameDisambiguator> disambiguator,
                          const QString& outDir,
                          Qt::LayoutDirection layoutDirection);

  void performRelinking(const AbstractRelinker& relinker);

  Qt::LayoutDirection layoutDirection() const { return m_layoutDirection; }

  const QString& outDir() const { return m_outDir; }

  void setOutDir(const QString& outDir) { m_outDir = outDir; }

  FileNameDisambiguator* disambiguator() { return m_disambiguator.get(); }

  const FileNameDisambiguator* disambiguator() const { return m_disambiguator.get(); }

  // Output format settings
  OutputImageFormat outputFormat() const { return m_outputFormat; }
  void setOutputFormat(OutputImageFormat format) { m_outputFormat = format; }

  OutputTiffCompression tiffCompression() const { return m_tiffCompression; }
  void setTiffCompression(OutputTiffCompression compression) { m_tiffCompression = compression; }

  int jpegQuality() const { return m_jpegQuality; }
  void setJpegQuality(int quality) { m_jpegQuality = quality; }

  QString fileNameFor(const PageId& page) const;

  QString filePathFor(const PageId& page) const;

  // Find existing output file for a page, checking all supported extensions (.tif, .png, .jpg)
  // Returns the path if found, empty string if not found
  QString findExistingOutputFile(const PageId& page) const;

  // Get file extension for current format (without dot)
  QString formatExtension() const;

 private:
  std::shared_ptr<FileNameDisambiguator> m_disambiguator;
  QString m_outDir;
  Qt::LayoutDirection m_layoutDirection;
  OutputImageFormat m_outputFormat = OutputImageFormat::TIFF;
  OutputTiffCompression m_tiffCompression = OutputTiffCompression::LZW;
  int m_jpegQuality = 90;
};


#endif  // ifndef SCANTAILOR_CORE_OUTPUTFILENAMEGENERATOR_H_
