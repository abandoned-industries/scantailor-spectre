// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "Task.h"

#include <QDebug>
#include <QFile>
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

  // First, delegate to output task for actual image processing
  // This creates the output TIFF file that we'll OCR
  FilterResultPtr outputResult;
  if (m_outputTask) {
    outputResult = m_outputTask->process(status, data, contentRectPhys);
  }

  status.throwIfCancelled();

  // Now perform OCR on the OUTPUT image (not the original)
  // This ensures bounding boxes match what's in the PDF
  if (m_settings->ocrEnabled()) {
    // Load the output file that was just created
    const QString outputFilePath = m_outFileNameGen.filePathFor(m_pageId);
    qDebug() << "OCR: Checking output file" << outputFilePath << "exists:" << QFile::exists(outputFilePath);

    if (!m_settings->hasOcrResult(m_pageId)) {
      if (QFile::exists(outputFilePath)) {
        QImage outputImage(outputFilePath);
        if (!outputImage.isNull()) {
          qDebug() << "OCR: Running OCR on output image" << outputFilePath
                   << "size:" << outputImage.width() << "x" << outputImage.height();
          performOcr(outputImage);
        } else {
          qWarning() << "OCR: Failed to load output image" << outputFilePath;
        }
      } else {
        qWarning() << "OCR: Output file not found" << outputFilePath;
      }
    } else {
      // Check if cached result has valid dimensions (non-zero means it was done on output image)
      const std::unique_ptr<OcrResult> cached = m_settings->getOcrResult(m_pageId);
      if (cached && cached->imageWidth() > 0 && cached->imageHeight() > 0) {
        qDebug() << "OCR: Using cached result for page" << m_pageId.imageId().filePath()
                 << "words:" << cached->words().size()
                 << "dims:" << cached->imageWidth() << "x" << cached->imageHeight();
      } else {
        // Invalid cached result - clear and re-run
        qDebug() << "OCR: Clearing invalid cached result for page" << m_pageId.imageId().filePath();
        m_settings->clearOcrResult(m_pageId);
        if (QFile::exists(outputFilePath)) {
          QImage outputImage(outputFilePath);
          if (!outputImage.isNull()) {
            qDebug() << "OCR: Re-running OCR on output image" << outputFilePath;
            performOcr(outputImage);
          }
        }
      }
    }
  }

  // Wrap the result so filter() returns the OCR filter, not output
  return std::make_shared<UiUpdater>(m_filter, m_pageId, outputResult);
}

void Task::performOcr(const QImage& image) {
  if (!AppleVisionDetector::isAvailable()) {
    qWarning() << "OCR: Apple Vision not available on this platform";
    return;
  }

  AppleVisionDetector::OcrConfig config;
  config.languageCode = m_settings->language();
  config.useAccurateRecognition = m_settings->useAccurateRecognition();
  config.usesLanguageCorrection = true;

  qDebug() << "OCR: Performing OCR with language:" << (config.languageCode.isEmpty() ? "auto-detect" : config.languageCode)
           << "accurate:" << config.useAccurateRecognition;

  const QVector<AppleVisionDetector::OcrWordResult> words = AppleVisionDetector::performOcr(image, config);

  OcrResult result;
  result.setImageDimensions(image.width(), image.height());
  result.setLanguage(config.languageCode);

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
