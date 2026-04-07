// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "CacheDrivenTask.h"

#include <core/ApplicationSettings.h>

#include <utility>

#include "IncompleteThumbnail.h"
#include "PageInfo.h"
#include "Settings.h"
#include "Thumbnail.h"
#include "core/AbstractFilterDataCollector.h"
#include "core/ThumbnailCollector.h"
#include "filters/select_content/CacheDrivenTask.h"

namespace page_box {
CacheDrivenTask::CacheDrivenTask(std::shared_ptr<Settings> settings,
                                 std::shared_ptr<select_content::CacheDrivenTask> nextTask)
    : m_settings(std::move(settings)), m_nextTask(std::move(nextTask)) {}

CacheDrivenTask::~CacheDrivenTask() = default;

void CacheDrivenTask::process(const PageInfo& pageInfo,
                              AbstractFilterDataCollector* collector,
                              const ImageTransformation& xform) {
  std::unique_ptr<Params> params(m_settings->getPageParams(pageInfo.id()));
  const Dependencies deps = params
      ? Dependencies(xform.resultingPreCropArea(), params->pageDetectionMode(), params->isFineTuningEnabled())
      : Dependencies(xform.resultingPreCropArea());

  if (!params || !deps.compatibleWith(params->dependencies())) {
    if (auto* thumbCol = dynamic_cast<ThumbnailCollector*>(collector)) {
      thumbCol->processThumbnail(std::unique_ptr<QGraphicsItem>(new IncompleteThumbnail(
          thumbCol->thumbnailCache(), thumbCol->maxLogicalThumbSize(), pageInfo.imageId(), xform)));
    }
    return;
  }

  if (m_nextTask) {
    m_nextTask->process(pageInfo, collector, xform);
    return;
  }

  ApplicationSettings& appSettings = ApplicationSettings::getInstance();
  const double deviationCoef = appSettings.getSelectContentDeviationCoef();
  const double deviationThreshold = appSettings.getSelectContentDeviationThreshold();

  if (auto* thumbCol = dynamic_cast<ThumbnailCollector*>(collector)) {
    thumbCol->processThumbnail(std::unique_ptr<QGraphicsItem>(new Thumbnail(
        thumbCol->thumbnailCache(), thumbCol->maxLogicalThumbSize(), pageInfo.imageId(), xform, params->pageRect(),
        m_settings->deviationProvider().isDeviant(pageInfo.id(), deviationCoef, deviationThreshold, true))));
  }
}
}  // namespace page_box
