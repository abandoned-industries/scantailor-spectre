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
#include "filters/ocr/Task.h"

namespace export_ {

/**
 * Wrapper around OCR's FilterResult that returns the export filter
 * from filter() so the UI stays on the Export stage.
 */
class Task::UiUpdater : public FilterResult {
 public:
  UiUpdater(std::shared_ptr<Filter> filter, const PageId& pageId, FilterResultPtr ocrResult)
      : m_filter(std::move(filter)), m_pageId(pageId), m_ocrResult(std::move(ocrResult)) {}

  void updateUI(FilterUiInterface* ui) override {
    // Delegate to OCR's updateUI FIRST - this shows the output image view
    if (m_ocrResult) {
      m_ocrResult->updateUI(ui);
    }

    // THEN override with our options widget
    OptionsWidget* optWidget = m_filter->optionsWidget();
    optWidget->postUpdateUI(m_pageId);
    ui->setOptionsWidget(optWidget, ui->KEEP_OWNERSHIP);
  }

  std::shared_ptr<AbstractFilter> filter() override { return m_filter; }

 private:
  std::shared_ptr<Filter> m_filter;
  PageId m_pageId;
  FilterResultPtr m_ocrResult;
};

Task::Task(std::shared_ptr<Filter> filter,
           std::shared_ptr<ocr::Task> ocrTask,
           const PageId& pageId,
           const bool batch)
    : m_filter(std::move(filter)),
      m_ocrTask(std::move(ocrTask)),
      m_pageId(pageId),
      m_batchProcessing(batch) {}

Task::~Task() = default;

FilterResultPtr Task::process(const TaskStatus& status, const FilterData& data, const QPolygonF& contentRectPhys) {
  status.throwIfCancelled();

  // Export filter delegates to OCR task (which delegates to output)
  if (m_ocrTask) {
    FilterResultPtr ocrResult = m_ocrTask->process(status, data, contentRectPhys);
    // Wrap the result so filter() returns the export filter, not OCR
    return std::make_shared<UiUpdater>(m_filter, m_pageId, ocrResult);
  }

  // Should never reach here - OCR task should always be present
  return nullptr;
}

}  // namespace export_
