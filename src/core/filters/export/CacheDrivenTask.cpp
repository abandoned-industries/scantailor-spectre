// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "CacheDrivenTask.h"

#include <utility>

#include "filters/output/CacheDrivenTask.h"

namespace export_ {

CacheDrivenTask::CacheDrivenTask(std::shared_ptr<output::CacheDrivenTask> outputTask)
    : m_outputTask(std::move(outputTask)) {}

CacheDrivenTask::~CacheDrivenTask() = default;

void CacheDrivenTask::process(const PageInfo& pageInfo,
                              AbstractFilterDataCollector* collector,
                              const ImageTransformation& xform,
                              const QPolygonF& contentRectPhys) {
  // Delegate to output filter's cache-driven task for thumbnail generation
  // Export filter reuses the same thumbnails as the Output filter
  if (m_outputTask) {
    m_outputTask->process(pageInfo, collector, xform, contentRectPhys);
  }
}

}  // namespace export_
