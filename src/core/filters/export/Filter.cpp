// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "Filter.h"

#include <filters/output/CacheDrivenTask.h>
#include <filters/output/Task.h>

#include <utility>

#include "CacheDrivenTask.h"
#include "FilterUiInterface.h"
#include "OptionsWidget.h"
#include "ProjectPages.h"
#include "ProjectReader.h"
#include "ProjectWriter.h"
#include "Settings.h"
#include "Task.h"
#include "filters/output/Settings.h"

namespace export_ {
Filter::Filter(std::shared_ptr<ProjectPages> pages, const PageSelectionAccessor& pageSelectionAccessor)
    : m_pages(std::move(pages)), m_settings(std::make_shared<Settings>()), m_selectedPageOrder(0) {
  m_optionsWidget.reset(new OptionsWidget(m_settings, pageSelectionAccessor));

  const PageOrderOption::ProviderPtr defaultOrder;
  m_pageOrderOptions.emplace_back(tr("Natural order"), defaultOrder);
}

Filter::~Filter() = default;

QString Filter::getName() const {
  return tr("Export");
}

PageView Filter::getView() const {
  return PAGE_VIEW;
}

void Filter::selected() {
  // Nothing to do
}

int Filter::selectedPageOrder() const {
  return m_selectedPageOrder;
}

void Filter::selectPageOrder(int option) {
  assert((unsigned) option < m_pageOrderOptions.size());
  m_selectedPageOrder = option;
}

std::vector<PageOrderOption> Filter::pageOrderOptions() const {
  return m_pageOrderOptions;
}

void Filter::performRelinking(const AbstractRelinker& relinker) {
  // Export filter doesn't need relinking - it has no per-page settings
}

void Filter::preUpdateUI(FilterUiInterface* ui, const PageInfo& pageInfo) {
  m_optionsWidget->preUpdateUI(pageInfo);
  ui->setOptionsWidget(m_optionsWidget.get(), ui->KEEP_OWNERSHIP);
}

QDomElement Filter::saveSettings(const ProjectWriter& writer, QDomDocument& doc) const {
  QDomElement filterEl(doc.createElement("export"));

  // Save global export settings
  filterEl.setAttribute("noDpiLimit", m_settings->noDpiLimit() ? "1" : "0");
  filterEl.setAttribute("maxDpi", m_settings->maxDpi());
  filterEl.setAttribute("compressGrayscale", m_settings->compressGrayscale() ? "1" : "0");
  filterEl.setAttribute("quality", static_cast<int>(m_settings->quality()));

  return filterEl;
}

void Filter::loadSettings(const ProjectReader& reader, const QDomElement& filtersEl) {
  const QDomElement filterEl(filtersEl.namedItem("export").toElement());
  if (filterEl.isNull()) {
    return;
  }

  m_settings->setNoDpiLimit(filterEl.attribute("noDpiLimit", "0") == "1");
  m_settings->setMaxDpi(filterEl.attribute("maxDpi", "400").toInt());
  m_settings->setCompressGrayscale(filterEl.attribute("compressGrayscale", "0") == "1");
  m_settings->setQuality(static_cast<PdfExporter::Quality>(filterEl.attribute("quality", "1").toInt()));
}

void Filter::loadDefaultSettings(const PageInfo& pageInfo) {
  // No per-page default settings needed
}

std::shared_ptr<Task> Filter::createTask(const PageId& pageId,
                                         std::shared_ptr<output::Task> outputTask,
                                         const bool batch) {
  return std::make_shared<Task>(std::static_pointer_cast<Filter>(shared_from_this()), std::move(outputTask), pageId,
                                batch);
}

std::shared_ptr<CacheDrivenTask> Filter::createCacheDrivenTask(std::shared_ptr<output::CacheDrivenTask> outputTask) {
  return std::make_shared<CacheDrivenTask>(std::move(outputTask));
}

OptionsWidget* Filter::optionsWidget() {
  return m_optionsWidget.get();
}

void Filter::setOutputSettings(std::shared_ptr<output::Settings> outputSettings) {
  m_optionsWidget->setOutputSettings(std::move(outputSettings));
}
}  // namespace export_
