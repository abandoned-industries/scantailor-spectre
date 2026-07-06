// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "BookLookup.h"

#include <QByteArray>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

namespace {
const char* const kUserAgent = "ScanTailorSpectre/2.0";

// Digits only, but preserve a trailing X (ISBN-10 check digit), uppercased.
QString normalizeIsbn(const QString& raw) {
  QString out;
  for (const QChar& c : raw) {
    if (c.isDigit()) {
      out.append(c);
    } else if (c == QLatin1Char('X') || c == QLatin1Char('x')) {
      out.append(QLatin1Char('X'));
    }
  }
  return out;
}

// Extract the first 4-digit year (15xx-20xx) from a free-form publish date such
// as "2000", "June 2000" or "1992-06-01".
QString extractYear(const QString& date) {
  static const QRegularExpression yearRe(QStringLiteral("\\b(1[5-9]\\d{2}|20\\d{2})\\b"));
  const QRegularExpressionMatch m = yearRe.match(date);
  return m.hasMatch() ? m.captured(1) : QString();
}
}  // namespace

BookLookup::BookLookup(QObject* parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this)) {}

BookLookup::~BookLookup() = default;

QByteArray BookLookup::fetch(const QString& url, int timeoutMs, bool& timedOut, bool& networkError) {
  timedOut = false;
  networkError = false;

  QNetworkRequest request((QUrl(url)));
  request.setRawHeader("User-Agent", kUserAgent);

  QNetworkReply* reply = m_networkManager->get(request);

  QEventLoop loop;
  QTimer timer;
  timer.setSingleShot(true);
  connect(&timer, &QTimer::timeout, &loop, [&loop, &timedOut, reply]() {
    timedOut = true;
    reply->abort();
    loop.quit();
  });
  connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  timer.start(timeoutMs);
  loop.exec();

  QByteArray body;
  if (!timedOut && reply->error() == QNetworkReply::NoError) {
    body = reply->readAll();
  } else if (!timedOut) {
    networkError = true;
  }
  reply->deleteLater();
  return body;
}

bool BookLookup::parseOpenLibrary(const QByteArray& body, const QString& isbn, BookMetadata& out) {
  const QJsonDocument doc = QJsonDocument::fromJson(body);
  if (!doc.isObject()) {
    return false;
  }
  const QJsonObject root = doc.object();
  // Response is keyed by "ISBN:<isbn>"; an empty root means "not found".
  const QString key = QStringLiteral("ISBN:") + isbn;
  const QJsonObject rec = root.value(key).toObject();
  if (rec.isEmpty()) {
    return false;
  }

  QString title = rec.value("title").toString();
  if (title.isEmpty()) {
    return false;
  }
  const QString subtitle = rec.value("subtitle").toString();
  if (!subtitle.isEmpty()) {
    title += QStringLiteral(": ") + subtitle;
  }
  out.title = title;

  QStringList authorNames;
  for (const QJsonValue& a : rec.value("authors").toArray()) {
    const QString name = a.toObject().value("name").toString();
    if (!name.isEmpty()) {
      authorNames << name;
    }
  }
  if (!authorNames.isEmpty()) {
    out.authors = authorNames.join(QStringLiteral("; "));
  }

  const QString year = extractYear(rec.value("publish_date").toString());
  if (!year.isEmpty()) {
    out.year = year;
  }

  const QJsonArray publishers = rec.value("publishers").toArray();
  if (!publishers.isEmpty()) {
    const QString name = publishers.first().toObject().value("name").toString();
    if (!name.isEmpty()) {
      out.publisher = name;
    }
  }

  const QJsonArray places = rec.value("publish_places").toArray();
  if (!places.isEmpty()) {
    const QString name = places.first().toObject().value("name").toString();
    if (!name.isEmpty()) {
      out.place = name;
    }
  }

  out.isbn = isbn;
  return true;
}

bool BookLookup::mergeGoogleBooks(const QByteArray& body, const QString& isbn, BookMetadata& out) {
  const QJsonDocument doc = QJsonDocument::fromJson(body);
  if (!doc.isObject()) {
    return false;
  }
  const QJsonArray items = doc.object().value("items").toArray();
  if (items.isEmpty()) {
    return false;
  }
  const QJsonObject info = items.first().toObject().value("volumeInfo").toObject();
  if (info.isEmpty()) {
    return false;
  }

  // Language is what Google Books adds over OpenLibrary; always take it.
  const QString language = info.value("language").toString();
  if (!language.isEmpty()) {
    out.language = language;
  }

  // Fill only fields OpenLibrary left empty — don't overwrite its (usually
  // richer) values.
  if (out.title.isEmpty()) {
    QString title = info.value("title").toString();
    const QString subtitle = info.value("subtitle").toString();
    if (!title.isEmpty() && !subtitle.isEmpty()) {
      title += QStringLiteral(": ") + subtitle;
    }
    if (!title.isEmpty()) {
      out.title = title;
    }
  }
  if (out.authors.isEmpty()) {
    QStringList authorNames;
    for (const QJsonValue& a : info.value("authors").toArray()) {
      const QString name = a.toString();
      if (!name.isEmpty()) {
        authorNames << name;
      }
    }
    if (!authorNames.isEmpty()) {
      out.authors = authorNames.join(QStringLiteral("; "));
    }
  }
  if (out.year.isEmpty()) {
    const QString year = extractYear(info.value("publishedDate").toString());
    if (!year.isEmpty()) {
      out.year = year;
    }
  }
  if (out.publisher.isEmpty()) {
    const QString publisher = info.value("publisher").toString();
    if (!publisher.isEmpty()) {
      out.publisher = publisher;
    }
  }

  out.isbn = isbn;
  return true;
}

BookLookup::Result BookLookup::lookupByIsbn(const QString& isbn, BookMetadata& out, int timeoutMs) {
  const QString norm = normalizeIsbn(isbn);
  if (norm.isEmpty()) {
    return {Status::NotFound, tr("No ISBN to look up.")};
  }

  // --- Primary: OpenLibrary ---
  bool timedOut = false;
  bool networkError = false;
  const QString olUrl = QStringLiteral(
                            "https://openlibrary.org/api/books?bibkeys=ISBN:%1&format=json&jscmd=data")
                            .arg(norm);
  const QByteArray olBody = fetch(olUrl, timeoutMs, timedOut, networkError);
  if (timedOut) {
    return {Status::Timeout, tr("The book database did not respond in time.")};
  }

  bool haveRecord = false;
  if (!networkError) {
    haveRecord = parseOpenLibrary(olBody, norm, out);
  }

  // --- Google Books: enrich (language + fill-ins) or act as fallback ---
  const QString gbUrl =
      QStringLiteral("https://www.googleapis.com/books/v1/volumes?q=isbn:%1").arg(norm);
  bool gbTimedOut = false;
  bool gbNetworkError = false;
  const QByteArray gbBody = fetch(gbUrl, timeoutMs, gbTimedOut, gbNetworkError);
  bool gbRecord = false;
  if (!gbTimedOut && !gbNetworkError) {
    gbRecord = mergeGoogleBooks(gbBody, norm, out);
  }

  if (haveRecord || gbRecord) {
    return {Status::Ok, tr("Found metadata for ISBN %1.").arg(norm)};
  }

  // Neither source had a usable record.
  if (networkError && (gbTimedOut || gbNetworkError)) {
    return {Status::NetworkError, tr("Could not reach the book database.")};
  }
  return {Status::NotFound, tr("No record found for ISBN %1.").arg(norm)};
}
