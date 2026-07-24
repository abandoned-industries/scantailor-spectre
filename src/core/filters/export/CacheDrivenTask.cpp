// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "CacheDrivenTask.h"

#include <utility>

#include "filters/ocr/CacheDrivenTask.h"

namespace export_ {

CacheDrivenTask::CacheDrivenTask(std::shared_ptr<ocr::CacheDrivenTask> ocrTask)
    : m_ocrTask(std::move(ocrTask)) {}

CacheDrivenTask::~CacheDrivenTask() = default;

void CacheDrivenTask::process(const PageInfo& pageInfo,
                              AbstractFilterDataCollector* collector,
                              const ImageTransformation& xform,
                              const QPolygonF& contentRectPhys) {
  // Delegate to OCR filter's cache-driven task (which delegates to output)
  if (m_ocrTask) {
    m_ocrTask->process(pageInfo, collector, xform, contentRectPhys);
  }
}

}  // namespace export_
