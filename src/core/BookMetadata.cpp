// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "BookMetadata.h"

#include <QRegularExpression>

namespace {
QString sanitizeFileNameComponent(const QString& input) {
  QString out;
  out.reserve(input.size());
  for (const QChar ch : input) {
    if (ch == '/' || ch == '\\' || ch == ':' || ch == '*' || ch == '?' || ch == '"' || ch == '<' || ch == '>'
        || ch == '|' || ch.category() == QChar::Other_Control) {
      out += ' ';
    } else {
      out += ch;
    }
  }
  out = out.simplified();
  while (out.startsWith('.')) {
    out.remove(0, 1);
  }
  while (out.endsWith('.')) {
    out.chop(1);
  }
  out = out.trimmed();
  if (out.size() > 120) {
    out = out.left(120).trimmed();
  }
  return out;
}
}  // namespace

QString creatorRoleToString(CreatorRole role) {
  switch (role) {
    case CreatorRole::Editor:
      return QStringLiteral("editor");
    case CreatorRole::Translator:
      return QStringLiteral("translator");
    case CreatorRole::Author:
      break;
  }
  return QStringLiteral("author");
}

CreatorRole creatorRoleFromString(const QString& str) {
  if (str == QLatin1String("editor")) {
    return CreatorRole::Editor;
  }
  if (str == QLatin1String("translator")) {
    return CreatorRole::Translator;
  }
  return CreatorRole::Author;
}

QString BookMetadata::zoteroCreatorType() const {
  return creatorRoleToString(creatorRole);
}

QList<BookMetadata::Creator> BookMetadata::parseCreators() const {
  QList<Creator> creators;
  const QStringList parts = authors.split(';');
  for (const QString& part : parts) {
    const QString name = part.trimmed();
    if (name.isEmpty()) {
      continue;
    }

    const QStringList tokens = name.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    Creator creator;
    if (tokens.size() == 1) {
      creator.lastName = tokens.front();
    } else {
      creator.lastName = tokens.back();
      creator.firstName = QStringList(tokens.mid(0, tokens.size() - 1)).join(' ');
    }
    creators.append(creator);
  }
  return creators;
}

QString buildRecommendedPdfFileName(const BookMetadata& meta, const QString& fallbackBaseName) {
  if (meta.title.trimmed().isEmpty()) {
    const QString fallback = sanitizeFileNameComponent(fallbackBaseName);
    if (fallback.isEmpty()) {
      return QStringLiteral("output.pdf");
    }
    return fallback + ".pdf";
  }

  QString base;
  if (meta.authors.trimmed().isEmpty()) {
    base = meta.title;
  } else {
    base = meta.authors + " - " + meta.title;
  }
  if (!meta.year.trimmed().isEmpty()) {
    base += " (" + meta.year + ")";
  }

  const QString sanitized = sanitizeFileNameComponent(base);
  if (sanitized.isEmpty()) {
    return QStringLiteral("output.pdf");
  }
  return sanitized + ".pdf";
}
