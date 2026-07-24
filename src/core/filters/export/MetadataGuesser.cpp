// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "MetadataGuesser.h"

#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QVector>
#include <algorithm>
#include <memory>

#include "PageId.h"
#include "PageSequence.h"
#include "filters/ocr/OcrResult.h"
#include "filters/ocr/Settings.h"

namespace export_ {

namespace {

// A clustered line of OCR words, in top-left image pixel coordinates.
struct Line {
  QString text;
  double meanHeight = 0.0;
  double meanCenterY = 0.0;
  double top = 0.0;
  double meanConfidence = 0.0;
};

double medianOf(std::vector<double> values) {
  if (values.empty()) {
    return 0.0;
  }
  std::sort(values.begin(), values.end());
  const size_t mid = values.size() / 2;
  if (values.size() % 2 == 0) {
    return 0.5 * (values[mid - 1] + values[mid]);
  }
  return values[mid];
}

// Cluster words into lines by vertical center. Words are visited in order of
// increasing center-y and appended to the current line while their center-y
// stays within `tolerance` of the line's running mean center-y. `tolerance`
// is derived from the median word height (~0.6x) so it adapts to the page's
// type size rather than using an absolute pixel threshold.
QVector<Line> clusterLines(const ocr::OcrResult& result) {
  QVector<Line> lines;

  const QVector<ocr::OcrWord>& words = result.words();
  if (words.isEmpty()) {
    return lines;
  }

  std::vector<double> heights;
  heights.reserve(words.size());
  for (const ocr::OcrWord& w : words) {
    if (w.boundingBox.height() > 0.0) {
      heights.push_back(w.boundingBox.height());
    }
  }
  const double medianHeight = medianOf(heights);
  if (medianHeight <= 0.0) {
    return lines;
  }
  const double tolerance = 0.6 * medianHeight;

  // Sort a copy of the words by vertical center.
  QVector<ocr::OcrWord> sorted = words;
  std::sort(sorted.begin(), sorted.end(), [](const ocr::OcrWord& a, const ocr::OcrWord& b) {
    return a.boundingBox.center().y() < b.boundingBox.center().y();
  });

  // Each running line collects the raw words so we can finalize (sort by left,
  // join, compute means) once the line is complete.
  QVector<QVector<ocr::OcrWord>> rawLines;
  double runningMeanCenterY = 0.0;
  for (const ocr::OcrWord& w : sorted) {
    const double centerY = w.boundingBox.center().y();
    if (!rawLines.isEmpty() && std::abs(centerY - runningMeanCenterY) <= tolerance) {
      QVector<ocr::OcrWord>& line = rawLines.last();
      line.push_back(w);
      double sumCenterY = 0.0;
      for (const ocr::OcrWord& lw : line) {
        sumCenterY += lw.boundingBox.center().y();
      }
      runningMeanCenterY = sumCenterY / line.size();
    } else {
      rawLines.push_back(QVector<ocr::OcrWord>{w});
      runningMeanCenterY = centerY;
    }
  }

  for (QVector<ocr::OcrWord>& raw : rawLines) {
    std::sort(raw.begin(), raw.end(), [](const ocr::OcrWord& a, const ocr::OcrWord& b) {
      return a.boundingBox.left() < b.boundingBox.left();
    });

    Line line;
    QStringList parts;
    double sumHeight = 0.0;
    double sumCenterY = 0.0;
    double sumConfidence = 0.0;
    double minTop = 0.0;
    bool firstWord = true;
    for (const ocr::OcrWord& w : raw) {
      parts << w.text;
      sumHeight += w.boundingBox.height();
      sumCenterY += w.boundingBox.center().y();
      sumConfidence += w.confidence;
      if (firstWord || w.boundingBox.top() < minTop) {
        minTop = w.boundingBox.top();
        firstWord = false;
      }
    }
    const int count = raw.size();
    line.text = parts.join(QLatin1Char(' ')).trimmed();
    line.meanHeight = sumHeight / count;
    line.meanCenterY = sumCenterY / count;
    line.meanConfidence = sumConfidence / count;
    line.top = minTop;
    lines.push_back(line);
  }

  return lines;
}

bool containsLetter(const QString& text) {
  for (const QChar& c : text) {
    if (c.isLetter()) {
      return true;
    }
  }
  return false;
}

// Loose name shape: 1-4 capitalized tokens; letters, periods, apostrophes and
// hyphens allowed within a token.
bool looksLikeName(const QString& text) {
  const QString trimmed = text.trimmed();
  if (trimmed.isEmpty()) {
    return false;
  }
  const QStringList tokens = trimmed.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
  if (tokens.isEmpty() || tokens.size() > 4) {
    return false;
  }
  static const QRegularExpression tokenRe(QStringLiteral("^\\p{Lu}[\\p{L}.'-]*$"));
  for (const QString& token : tokens) {
    if (!tokenRe.match(token).hasMatch()) {
      return false;
    }
  }
  return true;
}

// Pick the title line: largest mean word height, top edge within the top 60%
// of the page, at least 3 chars, at least one letter, confidence >= 0.3.
// Ties on height are broken in favor of the line higher on the page.
int pickTitleLine(const QVector<Line>& lines, int imageHeight) {
  const double topLimit = imageHeight * 0.6;
  int best = -1;
  for (int i = 0; i < lines.size(); ++i) {
    const Line& line = lines[i];
    if (line.top > topLimit) {
      continue;
    }
    if (line.text.length() < 3) {
      continue;
    }
    if (!containsLetter(line.text)) {
      continue;
    }
    if (line.meanConfidence < 0.3) {
      continue;
    }
    if (best < 0 || line.meanHeight > lines[best].meanHeight ||
        (line.meanHeight == lines[best].meanHeight && line.top < lines[best].top)) {
      best = i;
    }
  }
  return best;
}

// A byline keyword introduces the author(s): "by", "edited by", etc. The
// author names either follow on the same line or begin on the next line.
const QRegularExpression& bylineRe() {
  static const QRegularExpression re(
      QStringLiteral("^\\s*(edited by|written by|translated by|by)\\b\\s*(.*)$"),
      QRegularExpression::CaseInsensitiveOption);
  return re;
}

// Convert an ALL-CAPS name to Title Case (e.g. "ERIC HOBSBAWM" -> "Eric
// Hobsbawm"), leaving connective words like "and" lowercase. Mixed-case text is
// returned unchanged so existing capitalization is preserved.
QString normalizeNameCase(const QString& text) {
  bool allUpper = true;
  for (const QChar& c : text) {
    if (c.isLetter() && c.isLower()) {
      allUpper = false;
      break;
    }
  }
  if (!allUpper) {
    return text;
  }
  const QStringList tokens = text.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
  QStringList out;
  for (const QString& tok : tokens) {
    const QString lower = tok.toLower();
    if (lower == QLatin1String("and") || lower == QLatin1String("with")) {
      out << lower;
      continue;
    }
    if (tok.isEmpty()) {
      continue;
    }
    out << tok.left(1).toUpper() + tok.mid(1).toLower();
  }
  return out.join(QLatin1Char(' '));
}

// A byline name line may contain lowercase connectives ("and", "with"), so it
// is looser than looksLikeName() but must still contain a capitalized token.
bool looksLikeNameLine(const QString& text) {
  const QString trimmed = text.trimmed();
  if (trimmed.isEmpty()) {
    return false;
  }
  static const QRegularExpression capToken(QStringLiteral("\\b\\p{Lu}[\\p{L}.'-]*"));
  return capToken.match(trimmed).hasMatch();
}

// Map a byline keyword ("edited by" / "translated by" / "by" / "written by")
// to the creator role that applies to the names it introduces.
CreatorRole roleFromBylineKeyword(const QString& keyword) {
  const QString lower = keyword.toLower();
  if (lower == QLatin1String("edited by")) {
    return CreatorRole::Editor;
  }
  if (lower == QLatin1String("translated by")) {
    return CreatorRole::Translator;
  }
  return CreatorRole::Author;
}

// Prefer an explicit byline ("by" / "edited by" / ...) over positional
// guessing. `titleLineIndices` are lines that were merged into the title block
// and must never be treated as the author. On a match, `*outRole` (if non-null)
// is set from the byline keyword. Returns empty if no byline is found.
QString pickAuthorFromByline(const QVector<Line>& lines, const QSet<int>& titleLineIndices,
                             CreatorRole* outRole) {
  for (int i = 0; i < lines.size(); ++i) {
    if (titleLineIndices.contains(i)) {
      continue;
    }
    const QRegularExpressionMatch m = bylineRe().match(lines[i].text.trimmed());
    if (!m.hasMatch()) {
      continue;
    }
    const CreatorRole role = roleFromBylineKeyword(m.captured(1).trimmed());
    QString names = m.captured(2).trimmed();
    // Names on the same line as the keyword (e.g. "by Jane Roe").
    if (!names.isEmpty() && looksLikeNameLine(names)) {
      if (outRole) {
        *outRole = role;
      }
      return normalizeNameCase(names);
    }
    // Keyword alone ("Edited by") -> names are on the following line(s).
    if (names.isEmpty()) {
      for (int j = i + 1; j < lines.size(); ++j) {
        if (titleLineIndices.contains(j)) {
          continue;
        }
        const QString next = lines[j].text.trimmed();
        if (next.isEmpty()) {
          continue;
        }
        if (bylineRe().match(next).hasMatch()) {
          break;  // a second byline keyword, stop
        }
        if (looksLikeNameLine(next)) {
          if (outRole) {
            *outRole = role;
          }
          return normalizeNameCase(next);
        }
        break;  // first non-empty follower is not name-shaped: give up
      }
    }
  }
  return QString();
}

// Positional fallback: first strictly-below, similarly-but-smaller line that is
// name-shaped and not part of the title block.
QString pickAuthorPositional(const QVector<Line>& lines, int titleIndex, const QSet<int>& titleLineIndices) {
  const double titleHeight = lines[titleIndex].meanHeight;
  const double titleCenterY = lines[titleIndex].meanCenterY;
  if (titleHeight <= 0.0) {
    return QString();
  }

  for (int i = 0; i < lines.size(); ++i) {
    if (i == titleIndex || titleLineIndices.contains(i)) {
      continue;
    }
    const Line& line = lines[i];
    if (line.meanCenterY <= titleCenterY) {
      continue;  // must be strictly below the title
    }
    const double ratio = line.meanHeight / titleHeight;
    if (ratio < 0.35 || ratio > 0.90) {
      continue;
    }

    QString candidate = line.text.trimmed();
    static const QRegularExpression byPrefix(QStringLiteral("^by\\s+"), QRegularExpression::CaseInsensitiveOption);
    candidate.remove(byPrefix);
    candidate = candidate.trimmed();

    if (looksLikeName(candidate)) {
      return normalizeNameCase(candidate);
    }
  }
  return QString();
}

// Merge the title's primary line with adjacent lines that belong to the same
// title block: vertically contiguous (gap small relative to line height) and of
// similar font size (mean word height within kTitleHeightTol). Merging stops at
// a clearly smaller line, a large vertical gap, or a byline keyword. Returns the
// merged title text and records the merged line indices in `mergedIndices`.
QString mergeTitleLines(const QVector<Line>& lines, int titleIndex, QSet<int>& mergedIndices) {
  // Lines within +/-35% height are "the same size"; a gap up to 2.5x the title
  // line height still reads as the same block. Tuned loose for display
  // typography, where real title blocks are set with generous leading.
  const double kTitleHeightTol = 0.35;
  const double kMaxGapFactor = 2.5;

  const double titleHeight = lines[titleIndex].meanHeight;

  // Order all lines top-to-bottom so "adjacent" is well defined.
  QVector<int> order;
  order.reserve(lines.size());
  for (int i = 0; i < lines.size(); ++i) {
    order.push_back(i);
  }
  std::sort(order.begin(), order.end(),
            [&lines](int a, int b) { return lines[a].meanCenterY < lines[b].meanCenterY; });

  int titlePos = 0;
  for (int p = 0; p < order.size(); ++p) {
    if (order[p] == titleIndex) {
      titlePos = p;
      break;
    }
  }

  const auto sameBlock = [&](int idx, int refIdx) -> bool {
    const Line& cur = lines[idx];
    const Line& ref = lines[refIdx];
    if (cur.text.trimmed().isEmpty() || !containsLetter(cur.text)) {
      return false;
    }
    if (bylineRe().match(cur.text.trimmed()).hasMatch()) {
      return false;
    }
    if (titleHeight <= 0.0 || cur.meanHeight <= 0.0) {
      return false;
    }
    const double ratio = cur.meanHeight / titleHeight;
    if (ratio < 1.0 - kTitleHeightTol || ratio > 1.0 + kTitleHeightTol) {
      return false;
    }
    // Vertical gap between the near edges of the two lines.
    const double gap = std::abs(cur.top - ref.top) - std::max(cur.meanHeight, ref.meanHeight);
    if (gap > kMaxGapFactor * titleHeight) {
      return false;
    }
    return true;
  };

  QMap<double, QString> byCenter;  // keeps top-to-bottom order for the join
  mergedIndices.insert(titleIndex);
  byCenter.insert(lines[titleIndex].meanCenterY, lines[titleIndex].text.trimmed());

  // Walk downward from the title line.
  int ref = titleIndex;
  for (int p = titlePos + 1; p < order.size(); ++p) {
    const int idx = order[p];
    if (!sameBlock(idx, ref)) {
      break;
    }
    mergedIndices.insert(idx);
    byCenter.insert(lines[idx].meanCenterY, lines[idx].text.trimmed());
    ref = idx;
  }
  // Walk upward from the title line (in case OCR picked a lower line as primary).
  ref = titleIndex;
  for (int p = titlePos - 1; p >= 0; --p) {
    const int idx = order[p];
    if (!sameBlock(idx, ref)) {
      break;
    }
    mergedIndices.insert(idx);
    byCenter.insert(lines[idx].meanCenterY, lines[idx].text.trimmed());
    ref = idx;
  }

  return QStringList(byCenter.values()).join(QLatin1Char(' ')).trimmed();
}

// Validate an ISBN-13 check digit (mod-10 weighted 1,3,...). `digits` must be 13
// numeric chars.
bool isValidIsbn13(const QString& digits) {
  if (digits.size() != 13) {
    return false;
  }
  int sum = 0;
  for (int i = 0; i < 13; ++i) {
    const int d = digits[i].digitValue();
    if (d < 0) {
      return false;
    }
    sum += (i % 2 == 0) ? d : d * 3;
  }
  return sum % 10 == 0;
}

// Validate an ISBN-10 check digit (mod-11, final digit may be 'X' == 10).
bool isValidIsbn10(const QString& value) {
  if (value.size() != 10) {
    return false;
  }
  int sum = 0;
  for (int i = 0; i < 10; ++i) {
    const QChar c = value[i];
    int d;
    if (i == 9 && (c == QLatin1Char('X') || c == QLatin1Char('x'))) {
      d = 10;
    } else if (c.isDigit()) {
      d = c.digitValue();
    } else {
      return false;
    }
    sum += d * (10 - i);
  }
  return sum % 11 == 0;
}

// Extract a normalized ISBN (digits only, trailing X preserved, uppercase) from
// the given text. Prefers a valid ISBN-13 (978/979) over a valid ISBN-10.
// Returns empty if nothing valid is found.
QString findIsbn(const QString& text) {
  // Candidate spans: an optional "ISBN"/"ISBN-13"/"e-ISBN" label, then a run of
  // 10-13 digits interspersed with spaces/hyphens, ending in a digit or X. The
  // bare form (no label) is only accepted for 978/979-prefixed ISBN-13 below.
  static const QRegularExpression labelledRe(
      QStringLiteral("(?:e-\\s*)?ISBN(?:[- ]?1[03])?\\s*:?\\s*([0-9][0-9\\- ]{8,16}[0-9Xx])"),
      QRegularExpression::CaseInsensitiveOption);
  static const QRegularExpression bareRe(
      QStringLiteral("\\b(97[89][0-9\\- ]{10,14}[0-9])\\b"));

  QString firstValid;

  const auto normalize = [](const QString& raw) -> QString {
    QString out;
    for (const QChar& c : raw) {
      if (c.isDigit()) {
        out.append(c);
      } else if (c == QLatin1Char('X') || c == QLatin1Char('x')) {
        out.append(QLatin1Char('X'));
      }
    }
    return out;
  };

  // Pass over labelled matches first (most reliable).
  QRegularExpressionMatchIterator it = labelledRe.globalMatch(text);
  while (it.hasNext()) {
    const QString norm = normalize(it.next().captured(1));
    if (norm.size() == 13 && isValidIsbn13(norm)) {
      return norm;  // prefer ISBN-13
    }
    if (firstValid.isEmpty() && norm.size() == 10 && isValidIsbn10(norm)) {
      firstValid = norm;
    }
  }

  // Then bare 978/979 13-digit runs with no explicit label.
  it = bareRe.globalMatch(text);
  while (it.hasNext()) {
    const QString norm = normalize(it.next().captured(1));
    if (norm.size() == 13 && isValidIsbn13(norm)) {
      return norm;
    }
  }

  return firstValid;
}

}  // namespace

MetadataGuess guessBookMetadata(const ocr::Settings& ocrSettings, const PageSequence& pages) {
  MetadataGuess guess;

  // Collect OCR results for the first ~8 pages, in page order. Title/author sit
  // on the title page; ISBN/copyright data usually lives on the verso/copyright
  // page but can be a few leaves in.
  std::vector<std::unique_ptr<ocr::OcrResult>> firstResults;
  const size_t limit = std::min<size_t>(8, pages.numPages());
  for (size_t i = 0; i < limit; ++i) {
    const PageId& pageId = pages.pageAt(i).id();
    if (ocrSettings.hasOcrResult(pageId)) {
      std::unique_ptr<ocr::OcrResult> result = ocrSettings.getOcrResult(pageId);
      if (result && !result->isEmpty()) {
        firstResults.push_back(std::move(result));
      }
    }
  }

  if (firstResults.empty()) {
    return guess;
  }

  // Title/author from the first page that yields a title candidate.
  for (const std::unique_ptr<ocr::OcrResult>& result : firstResults) {
    const int imageHeight = result->imageHeight();
    if (imageHeight <= 0) {
      continue;
    }
    const QVector<Line> lines = clusterLines(*result);
    const int titleIndex = pickTitleLine(lines, imageHeight);
    if (titleIndex < 0) {
      continue;
    }
    QSet<int> titleLineIndices;
    guess.title = mergeTitleLines(lines, titleIndex, titleLineIndices);

    // Prefer an explicit byline; fall back to the positional heuristic. Lines
    // merged into the title block are never eligible as the author. A matched
    // byline also fixes the creator role ("edited by" -> Editor, etc.).
    guess.author = pickAuthorFromByline(lines, titleLineIndices, &guess.creatorRole);
    if (guess.author.isEmpty()) {
      guess.author = pickAuthorPositional(lines, titleIndex, titleLineIndices);
    }
    break;
  }

  // Year: first match across the first OCR'd pages, in page order.
  static const QRegularExpression yearRe(QStringLiteral("\\b(1[5-9]\\d{2}|20\\d{2})\\b"));
  for (const std::unique_ptr<ocr::OcrResult>& result : firstResults) {
    const QRegularExpressionMatch match = yearRe.match(result->fullText());
    if (match.hasMatch()) {
      guess.year = match.captured(1);
      break;
    }
  }

  // ISBN: scan the front matter first (already loaded), then the last pages.
  // European imprints often print the ISBN on the final pages / back cover
  // rather than the copyright page. Prefer a valid ISBN-13 found anywhere over
  // an ISBN-10, but keep the first valid ISBN otherwise.
  const auto applyIsbn = [&guess](const QString& isbn) -> bool {
    if (isbn.isEmpty()) {
      return false;
    }
    guess.isbn = isbn;
    return isbn.size() == 13;  // ISBN-13 is the strongest signal; stop scanning
  };

  bool isbnDone = false;
  for (const std::unique_ptr<ocr::OcrResult>& result : firstResults) {
    if (applyIsbn(findIsbn(result->fullText()))) {
      isbnDone = true;
      break;
    }
  }
  if (!isbnDone) {
    const size_t n = pages.numPages();
    const size_t front = std::min<size_t>(8, n);         // covered by firstResults
    const size_t tailCount = std::min<size_t>(8, n);
    for (size_t i = (n > tailCount) ? n - tailCount : 0; i < n; ++i) {
      if (i < front) {
        continue;  // already scanned via the front window
      }
      const PageId& pageId = pages.pageAt(i).id();
      if (!ocrSettings.hasOcrResult(pageId)) {
        continue;
      }
      std::unique_ptr<ocr::OcrResult> result = ocrSettings.getOcrResult(pageId);
      if (!result || result->isEmpty()) {
        continue;
      }
      if (applyIsbn(findIsbn(result->fullText()))) {
        break;
      }
    }
  }

  return guess;
}

}  // namespace export_
