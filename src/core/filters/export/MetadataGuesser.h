// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_EXPORT_METADATAGUESSER_H_
#define SCANTAILOR_EXPORT_METADATAGUESSER_H_

#include <QString>

#include "BookMetadata.h"

class PageSequence;

namespace ocr {
class Settings;
}

namespace export_ {

struct MetadataGuess {
  QString title;
  QString author;
  QString year;
  QString isbn;  // Normalized: digits only (trailing X kept for ISBN-10).
  // Role inferred from the byline keyword ("edited by" -> Editor, etc.). Only
  // set when a byline actually matched; otherwise defaults to Author.
  CreatorRole creatorRole = CreatorRole::Author;
};

/**
 * Guess book metadata (title, author, year, isbn) from OCR results of the
 * first pages. Only fields that could be confidently derived are filled;
 * the rest are left empty. The caller is expected to apply the guess to
 * empty form fields only, so manual edits always win.
 */
MetadataGuess guessBookMetadata(const ocr::Settings& ocrSettings, const PageSequence& pages);

}  // namespace export_

#endif  // SCANTAILOR_EXPORT_METADATAGUESSER_H_
