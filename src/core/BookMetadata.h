// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_CORE_BOOKMETADATA_H_
#define SCANTAILOR_CORE_BOOKMETADATA_H_

#include <QList>
#include <QString>

// Role shared by all of the book's creators. Maps to Zotero's creatorType.
enum class CreatorRole { Author, Editor, Translator };

struct BookMetadata {
  QString title;
  QString authors;
  QString year;
  QString publisher;
  QString place;
  QString isbn;
  QString language;
  CreatorRole creatorRole = CreatorRole::Author;

  struct Creator {
    QString firstName;
    QString lastName;
  };

  bool isEmpty() const {
    return title.isEmpty() && authors.isEmpty() && year.isEmpty() && publisher.isEmpty() && place.isEmpty()
           && isbn.isEmpty() && language.isEmpty();
  }

  QList<Creator> parseCreators() const;

  // Zotero creatorType string ("author" / "editor" / "translator").
  QString zoteroCreatorType() const;
};

// CreatorRole <-> QString conversion for persistence. Unknown/empty -> Author.
QString creatorRoleToString(CreatorRole role);
CreatorRole creatorRoleFromString(const QString& str);

QString buildRecommendedPdfFileName(const BookMetadata& meta, const QString& fallbackBaseName);

#endif  // SCANTAILOR_CORE_BOOKMETADATA_H_
