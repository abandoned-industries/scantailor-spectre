// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_ABSTRACTOUTPUTTASK_H_
#define SCANTAILOR_ABSTRACTOUTPUTTASK_H_

#include <memory>

#include "FilterResult.h"

class TaskStatus;
class FilterData;
class QPolygonF;

/**
 * Abstract base class for output-stage tasks (output, ocr, export).
 * This allows finalize::Task to polymorphically call whichever task
 * is at the end of the processing chain.
 */
class AbstractOutputTask {
 public:
  virtual ~AbstractOutputTask() = default;

  virtual FilterResultPtr process(const TaskStatus& status,
                                  const FilterData& data,
                                  const QPolygonF& contentRectPhys) = 0;
};

#endif  // SCANTAILOR_ABSTRACTOUTPUTTASK_H_
