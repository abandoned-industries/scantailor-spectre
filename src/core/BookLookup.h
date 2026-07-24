// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_CORE_BOOKLOOKUP_H_
#define SCANTAILOR_CORE_BOOKLOOKUP_H_

#include <QObject>
#include <QString>

#include "BookMetadata.h"

class QNetworkAccessManager;

/**
 * Looks up canonical book metadata by ISBN from public online databases.
 *
 * OpenLibrary is the primary source (no key, generous rate limits); Google
 * Books is used to fill the language field (which OpenLibrary's jscmd=data
 * usually omits) and as a fallback when OpenLibrary has no record.
 *
 * All calls are blocking (nested QEventLoop with a single-shot timeout that
 * aborts the reply), and every failure is reported as a soft, human-readable
 * Result — never an exception.
 *
 * Endpoints used:
 *   - GET https://openlibrary.org/api/books?bibkeys=ISBN:<isbn>&format=json&jscmd=data
 *   - GET https://www.googleapis.com/books/v1/volumes?q=isbn:<isbn>
 */
class BookLookup : public QObject {
 public:
  enum class Status { Ok, NotFound, NetworkError, Timeout };

  struct Result {
    Status status;
    QString message;  // human-readable, already tr()'d

    bool ok() const { return status == Status::Ok; }
  };

  explicit BookLookup(QObject* parent = nullptr);
  ~BookLookup() override;

  /**
   * Blocking: normalize isbn to digits (keeping a trailing X), query
   * OpenLibrary, then enrich with Google Books. On success out is populated
   * (out.isbn is set to the normalized isbn). Never throws.
   */
  Result lookupByIsbn(const QString& isbn, BookMetadata& out, int timeoutMs = 8000);

 private:
  // Fetches url and returns the body; timedOut / networkError are set on failure.
  QByteArray fetch(const QString& url, int timeoutMs, bool& timedOut, bool& networkError);

  // Fills fields of out from an OpenLibrary record object. Returns true if the
  // record had at least a title.
  bool parseOpenLibrary(const QByteArray& body, const QString& isbn, BookMetadata& out);

  // Merges Google Books data into out: fills language always, and any still-empty
  // fields. Returns true if Google Books had a record.
  bool mergeGoogleBooks(const QByteArray& body, const QString& isbn, BookMetadata& out);

  QNetworkAccessManager* m_networkManager;
};

#endif  // ifndef SCANTAILOR_CORE_BOOKLOOKUP_H_
