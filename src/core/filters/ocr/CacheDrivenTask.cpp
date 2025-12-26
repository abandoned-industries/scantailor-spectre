// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "CacheDrivenTask.h"

#include <utility>

#include "Settings.h"
#include "filters/output/CacheDrivenTask.h"

namespace ocr {

CacheDrivenTask::CacheDrivenTask(std::shared_ptr<output::CacheDrivenTask> outputTask, std::shared_ptr<Settings> settings)
    : m_outputTask(std::move(outputTask)), m_settings(std::move(settings)) {}

CacheDrivenTask::~CacheDrivenTask() = default;

void CacheDrivenTask::process(const PageInfo& pageInfo,
                              AbstractFilterDataCollector* collector,
                              const ImageTransformation& xform,
                              const QPolygonF& contentRectPhys) {
  // OCR filter delegates to output filter's cache-driven task for thumbnail generation
  if (m_outputTask) {
    m_outputTask->process(pageInfo, collector, xform, contentRectPhys);
  }
}

}  // namespace ocr
