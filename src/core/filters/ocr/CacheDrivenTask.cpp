// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "CacheDrivenTask.h"

#include <QFileInfo>
#include <utility>

#include "ImageId.h"
#include "IncompleteThumbnail.h"
#include "PageInfo.h"
#include "Settings.h"
#include "Thumbnail.h"
#include "core/ThumbnailCollector.h"
#include "filters/output/CacheDrivenTask.h"
#include "filters/output/Params.h"
#include "filters/output/Settings.h"

namespace ocr {

CacheDrivenTask::CacheDrivenTask(std::shared_ptr<output::CacheDrivenTask> outputTask,
                                 std::shared_ptr<Settings> settings,
                                 const OutputFileNameGenerator& outFileNameGen,
                                 std::shared_ptr<output::Settings> outputSettings)
    : m_outputTask(std::move(outputTask)),
      m_settings(std::move(settings)),
      m_outFileNameGen(outFileNameGen),
      m_outputSettings(std::move(outputSettings)) {}

CacheDrivenTask::~CacheDrivenTask() = default;

void CacheDrivenTask::process(const PageInfo& pageInfo,
                              AbstractFilterDataCollector* collector,
                              const ImageTransformation& xform,
                              const QPolygonF& contentRectPhys) {
  if (auto* thumbCol = dynamic_cast<ThumbnailCollector*>(collector)) {
    // Get output file path - try to find existing file first
    QString outFilePath = m_outFileNameGen.findExistingOutputFile(pageInfo.id());
    if (outFilePath.isEmpty()) {
      outFilePath = m_outFileNameGen.filePathFor(pageInfo.id());
    }
    const QFileInfo outFileInfo(outFilePath);

    // Get output params for DPI
    const output::Params params(m_outputSettings->getParams(pageInfo.id()));

    // Create transform scaled to output DPI
    ImageTransformation newXform(xform);
    newXform.postScaleToDpi(params.outputDpi());

    if (!outFileInfo.exists()) {
      // Output not yet generated - show incomplete thumbnail
      thumbCol->processThumbnail(std::unique_ptr<QGraphicsItem>(new IncompleteThumbnail(
          thumbCol->thumbnailCache(), thumbCol->maxLogicalThumbSize(), pageInfo.imageId(), newXform)));
    } else {
      // Output exists - show processed image with OCR status overlay
      const ImageTransformation outXform(newXform.resultingRect(), params.outputDpi());
      thumbCol->processThumbnail(std::unique_ptr<QGraphicsItem>(
          new Thumbnail(thumbCol->thumbnailCache(), thumbCol->maxLogicalThumbSize(), ImageId(outFilePath), outXform,
                        m_settings, pageInfo.id())));
    }
  }
}

}  // namespace ocr
