// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "Task.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPolygonF>
#include <utility>

#include "AppleVisionDetector.h"
#include "Dpi.h"
#include "Filter.h"
#include "FilterData.h"
#include "FilterResult.h"
#include "FilterUiInterface.h"
#include "ImageView.h"
#include "OcrResult.h"
#include "OptionsWidget.h"
#include "Settings.h"
#include "TaskStatus.h"
#include "filters/output/Task.h"

namespace ocr {
namespace {
int ocrRecognitionSchema() {
  bool minimumHeightOk = false;
  const float minimumTextHeight =
      qEnvironmentVariable("SCANTAILOR_OCR_MIN_TEXT_HEIGHT").toFloat(&minimumHeightOk);
  minimumHeightOk = minimumHeightOk && minimumTextHeight >= 0.0f && minimumTextHeight <= 1.0f;

  bool maximumDimensionOk = false;
  const int maximumDimension =
      qEnvironmentVariableIntValue("SCANTAILOR_OCR_MAX_DIM", &maximumDimensionOk);
  maximumDimensionOk = maximumDimensionOk && maximumDimension > 0;

  bool correctionOverrideOk = false;
  const int correctionOverride =
      qEnvironmentVariableIntValue("SCANTAILOR_OCR_LANGUAGE_CORRECTION", &correctionOverrideOk);

  const bool grayscaleExperiment =
      qEnvironmentVariableIntValue("SCANTAILOR_VISION_GRAYSCALE8") != 0;
  const bool documentVision =
      qEnvironmentVariableIntValue("SCANTAILOR_DOCUMENT_VISION_OCR") != 0;
  if (!minimumHeightOk
      && !maximumDimensionOk
      && !correctionOverrideOk
      && !grayscaleExperiment
      && !documentVision) {
    // Schema 3 restores stable page-parallel Vision as the default and makes
    // true multilingual language detection explicit.
    return 3;
  }

  // Stable FNV-1a over every non-default recognition input. This is persisted
  // only as a cache dependency, not as a user-facing version number.
  quint32 hash = 2166136261u;
  const auto mix = [&hash](quint32 value) {
    for (int shift = 0; shift < 32; shift += 8) {
      hash ^= (value >> shift) & 0xffu;
      hash *= 16777619u;
    }
  };
  mix(3u);
  mix(minimumHeightOk ? static_cast<quint32>(qRound(minimumTextHeight * 100000.0f)) : 0u);
  mix(maximumDimensionOk ? static_cast<quint32>(maximumDimension) : 0u);
  mix(correctionOverrideOk ? static_cast<quint32>(correctionOverride != 0) : 2u);
  mix(static_cast<quint32>(grayscaleExperiment));
  mix(static_cast<quint32>(documentVision));
  const int schema = static_cast<int>(hash & 0x7fffffffu);
  return schema == 0 ? 3 : schema;
}

qint64 fileModificationTime(const QFileInfo& fileInfo) {
  return fileInfo.exists() ? fileInfo.lastModified().toMSecsSinceEpoch() : -1;
}
}  // namespace

/**
 * Wrapper around output's FilterResult that returns the OCR filter
 * from filter() so the UI stays on the OCR stage.
 */
class Task::UiUpdater : public FilterResult {
 public:
  UiUpdater(std::shared_ptr<Filter> filter, const PageId& pageId, FilterResultPtr outputResult)
      : m_filter(std::move(filter)), m_pageId(pageId), m_outputResult(std::move(outputResult)) {}

  void updateUI(FilterUiInterface* ui) override {
    // Delegate to output's updateUI FIRST to set up the image view
    if (m_outputResult) {
      m_outputResult->updateUI(ui);
    }

    // THEN override with our options widget (output's updateUI sets its own)
    OptionsWidget* optWidget = m_filter->optionsWidget();
    optWidget->postUpdateUI(m_pageId);
    ui->setOptionsWidget(optWidget, ui->KEEP_OWNERSHIP);

    ui->invalidateThumbnail(m_pageId);
  }

  std::shared_ptr<AbstractFilter> filter() override { return m_filter; }

 private:
  std::shared_ptr<Filter> m_filter;
  PageId m_pageId;
  FilterResultPtr m_outputResult;
};

Task::Task(std::shared_ptr<Filter> filter,
           std::shared_ptr<output::Task> outputTask,
           std::shared_ptr<Settings> settings,
           const PageId& pageId,
           const OutputFileNameGenerator& outFileNameGen,
           const bool /*batch*/)
    : m_filter(std::move(filter)),
      m_outputTask(std::move(outputTask)),
      m_settings(std::move(settings)),
      m_pageId(pageId),
      m_outFileNameGen(outFileNameGen) {}

Task::~Task() = default;

FilterResultPtr Task::process(const TaskStatus& status, const FilterData& data, const QPolygonF& contentRectPhys) {
  status.throwIfCancelled();

  qDebug() << "OCR STAGE: Processing page" << m_pageId.imageId().page() + 1
           << "file:" << m_pageId.imageId().filePath().section('/', -1);

  // First, delegate to output task for actual image processing
  // and retain the exact rendered / cache-loaded image for OCR.
  FilterResultPtr outputResult;
  QImage outputImage;
  if (m_outputTask) {
    outputResult = m_outputTask->process(status, data, contentRectPhys, &outputImage);
  }

  status.throwIfCancelled();

  // Now perform OCR on the OUTPUT image (not the original)
  // This ensures bounding boxes match what's in the PDF
  if (m_settings->ocrEnabled()) {
    const QString outputFilePath = m_outFileNameGen.filePathFor(m_pageId);
    qDebug() << "OCR: Checking output file" << outputFilePath << "exists:" << QFile::exists(outputFilePath);

    if (outputImage.isNull() && QFile::exists(outputFilePath)) {
      outputImage.load(outputFilePath);
      if (!outputImage.isNull()) {
        qDebug() << "OCR: Loaded fallback output image" << outputFilePath
                 << "size:" << outputImage.width() << "x" << outputImage.height();
      }
    }

    if (outputImage.isNull()) {
      qWarning() << "OCR: Output image unavailable" << outputFilePath;
    } else {
      const QFileInfo outputFileInfo(outputFilePath);
      const std::unique_ptr<OcrResult> cached = m_settings->getOcrResult(m_pageId);
      const bool cacheValid =
          cached
          && cached->recognitionSchema() == ocrRecognitionSchema()
          && cached->imageWidth() == outputImage.width()
          && cached->imageHeight() == outputImage.height()
          && cached->language() == m_settings->language()
          && cached->accurateRecognition() == m_settings->useAccurateRecognition()
          && cached->languageCorrection() == m_settings->useLanguageCorrection()
          && cached->outputFileSize() == (outputFileInfo.exists() ? outputFileInfo.size() : -1)
          && cached->outputFileMTime() == fileModificationTime(outputFileInfo);

      if (cacheValid) {
        qDebug() << "OCR: Using cached result for page" << m_pageId.imageId().filePath()
                 << "blocks:" << cached->words().size()
                 << "dims:" << cached->imageWidth() << "x" << cached->imageHeight();
      } else {
        if (cached) {
          qDebug() << "OCR: Invalidating stale result for page" << m_pageId.imageId().filePath();
          m_settings->clearOcrResult(m_pageId);
        }
        qDebug() << "OCR: Running OCR on output image"
                 << "size:" << outputImage.width() << "x" << outputImage.height();
        performOcr(outputImage, outputFileInfo);
      }
    }
  }

  // Wrap the result so filter() returns the OCR filter, not output
  return std::make_shared<UiUpdater>(m_filter, m_pageId, outputResult);
}

void Task::performOcr(const QImage& image,
                      const QFileInfo& outputFileInfo) {
  if (!AppleVisionDetector::isAvailable()) {
    qWarning() << "OCR: Apple Vision not available on this platform";
    return;
  }

  AppleVisionDetector::OcrConfig config;
  config.languageCode = m_settings->language();
  config.useAccurateRecognition = m_settings->useAccurateRecognition();
  config.usesLanguageCorrection = m_settings->useLanguageCorrection();

  qDebug() << "OCR: Performing OCR with language:"
           << (config.languageCode.isEmpty() ? "system-default" : config.languageCode)
           << "accurate:" << config.useAccurateRecognition;

  bool success = false;
  const QVector<AppleVisionDetector::OcrWordResult> words =
      AppleVisionDetector::performOcr(image, config, &success);
  if (!success) {
    qWarning() << "OCR: Vision request failed; result was not cached";
    return;
  }

  OcrResult result;
  result.setImageDimensions(image.width(), image.height());
  result.setLanguage(config.languageCode);
  result.setAccurateRecognition(config.useAccurateRecognition);
  result.setLanguageCorrection(config.usesLanguageCorrection);
  result.setOutputFileIdentity(
      outputFileInfo.exists() ? outputFileInfo.size() : -1,
      fileModificationTime(outputFileInfo));
  result.setRecognitionSchema(ocrRecognitionSchema());

  for (const auto& word : words) {
    OcrWord ocrWord;
    ocrWord.text = word.text;
    ocrWord.boundingBox = word.bounds;
    ocrWord.confidence = word.confidence;
    result.addWord(ocrWord);
  }

  qDebug() << "OCR: Found" << result.words().size() << "text blocks";

  m_settings->setOcrResult(m_pageId, result);
}

}  // namespace ocr
