// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "CacheDrivenTask.h"

#include <QPolygonF>
#include <utility>

#include "IncompleteThumbnail.h"
#include "PageInfo.h"
#include "Settings.h"
#include "Thumbnail.h"
#include "core/AbstractFilterDataCollector.h"
#include "core/ThumbnailCollector.h"
#include "filters/output/CacheDrivenTask.h"

namespace finalize {
CacheDrivenTask::CacheDrivenTask(std::shared_ptr<output::CacheDrivenTask> nextTask,
                                 std::shared_ptr<Settings> settings)
    : m_nextTask(std::move(nextTask)), m_settings(std::move(settings)) {}

CacheDrivenTask::~CacheDrivenTask() = default;

void CacheDrivenTask::process(const PageInfo& pageInfo,
                              AbstractFilterDataCollector* collector,
                              const ImageTransformation& xform,
                              const QPolygonF& contentRectPhys) {
  // Check if this page has been processed
  if (!m_settings->isProcessed(pageInfo.id())) {
    // Not processed yet - show incomplete thumbnail
    if (auto* thumbCol = dynamic_cast<ThumbnailCollector*>(collector)) {
      thumbCol->processThumbnail(std::unique_ptr<QGraphicsItem>(new IncompleteThumbnail(
          thumbCol->thumbnailCache(), thumbCol->maxLogicalThumbSize(), pageInfo.imageId(), xform)));
    }
    return;
  }

  // Delegate to output filter for thumbnail generation
  // Output filter shows the actual rendered output (B&W, grayscale, or color)
  if (m_nextTask) {
    m_nextTask->process(pageInfo, collector, xform, contentRectPhys);
    return;
  }

  // Fallback: generate finalize thumbnail with color mode label
  if (auto* thumbCol = dynamic_cast<ThumbnailCollector*>(collector)) {
    thumbCol->processThumbnail(std::unique_ptr<QGraphicsItem>(
        new Thumbnail(thumbCol->thumbnailCache(), thumbCol->maxLogicalThumbSize(), pageInfo.imageId(), xform,
                      m_settings, pageInfo.id())));
  }
}
}  // namespace finalize
