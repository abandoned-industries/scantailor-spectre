// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "Task.h"

#include <QPolygonF>
#include <utility>

#include "Filter.h"
#include "FilterData.h"
#include "FilterResult.h"
#include "FilterUiInterface.h"
#include "OptionsWidget.h"
#include "TaskStatus.h"
#include "filters/output/Task.h"

namespace export_ {

/**
 * Wrapper around output's FilterResult that returns the export filter
 * from filter() so the UI stays on the Export stage.
 */
class Task::UiUpdater : public FilterResult {
 public:
  UiUpdater(std::shared_ptr<Filter> filter, FilterResultPtr outputResult)
      : m_filter(std::move(filter)), m_outputResult(std::move(outputResult)) {}

  void updateUI(FilterUiInterface* ui) override {
    // Delegate to output's updateUI - this shows the output image view
    if (m_outputResult) {
      m_outputResult->updateUI(ui);
    }
  }

  std::shared_ptr<AbstractFilter> filter() override { return m_filter; }

 private:
  std::shared_ptr<Filter> m_filter;
  FilterResultPtr m_outputResult;
};

Task::Task(std::shared_ptr<Filter> filter,
           std::shared_ptr<output::Task> outputTask,
           const PageId& pageId,
           const bool batch)
    : m_filter(std::move(filter)),
      m_outputTask(std::move(outputTask)),
      m_pageId(pageId),
      m_batchProcessing(batch) {}

Task::~Task() = default;

FilterResultPtr Task::process(const TaskStatus& status, const FilterData& data, const QPolygonF& contentRectPhys) {
  status.throwIfCancelled();

  // Export filter always delegates to output task for actual processing
  // The export filter only provides the control panel, not actual image processing
  if (m_outputTask) {
    FilterResultPtr outputResult = m_outputTask->process(status, data, contentRectPhys);
    // Wrap the result so filter() returns the export filter, not output
    return std::make_shared<UiUpdater>(m_filter, outputResult);
  }

  // Should never reach here - output task should always be present
  return nullptr;
}

}  // namespace export_
