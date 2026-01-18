// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "Filter.h"

#include <filters/output/CacheDrivenTask.h>
#include <filters/output/Task.h>

#include <utility>

#include "CacheDrivenTask.h"
#include "FilterUiInterface.h"
#include "OcrResult.h"
#include "OptionsWidget.h"
#include "ProjectPages.h"
#include "ProjectReader.h"
#include "ProjectWriter.h"
#include "Settings.h"
#include "Task.h"

namespace ocr {
Filter::Filter(std::shared_ptr<ProjectPages> pages, const PageSelectionAccessor& pageSelectionAccessor)
    : m_pages(std::move(pages)), m_settings(std::make_shared<Settings>()), m_selectedPageOrder(0) {
  m_optionsWidget.reset(new OptionsWidget(m_settings, pageSelectionAccessor));

  const PageOrderOption::ProviderPtr defaultOrder;
  m_pageOrderOptions.emplace_back(tr("Natural order"), defaultOrder);
}

Filter::~Filter() = default;

QString Filter::getName() const {
  return tr("OCR");
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
  m_settings->performRelinking(relinker);
}

void Filter::preUpdateUI(FilterUiInterface* ui, const PageInfo& pageInfo) {
  m_optionsWidget->preUpdateUI(pageInfo);
  ui->setOptionsWidget(m_optionsWidget.get(), ui->KEEP_OWNERSHIP);
}

QDomElement Filter::saveSettings(const ProjectWriter& writer, QDomDocument& doc) const {
  QDomElement filterEl(doc.createElement("ocr"));

  // Save global OCR settings
  filterEl.setAttribute("enabled", m_settings->ocrEnabled() ? "1" : "0");
  filterEl.setAttribute("language", m_settings->language());
  filterEl.setAttribute("accurate", m_settings->useAccurateRecognition() ? "1" : "0");

  // Save per-page OCR results
  writer.enumPages(
      [&](const PageId& pageId, int numericId) { this->writePageSettings(doc, filterEl, pageId, numericId); });

  return filterEl;
}

void Filter::writePageSettings(QDomDocument& doc, QDomElement& filterEl, const PageId& pageId, int numericId) const {
  const std::unique_ptr<OcrResult> result = m_settings->getOcrResult(pageId);
  if (!result || result->isEmpty()) {
    return;
  }

  QDomElement pageEl(doc.createElement("page"));
  pageEl.setAttribute("id", numericId);
  pageEl.appendChild(result->toXml(doc, "ocr-result"));

  filterEl.appendChild(pageEl);
}

void Filter::loadSettings(const ProjectReader& reader, const QDomElement& filtersEl) {
  m_settings->clear();

  const QDomElement filterEl(filtersEl.namedItem("ocr").toElement());
  if (filterEl.isNull()) {
    return;
  }

  // Load global settings
  m_settings->setOcrEnabled(filterEl.attribute("enabled", "0") == "1");
  m_settings->setLanguage(filterEl.attribute("language", ""));
  m_settings->setUseAccurateRecognition(filterEl.attribute("accurate", "1") == "1");

  // Load per-page OCR results
  const QString pageTagName("page");
  QDomNode node(filterEl.firstChild());
  for (; !node.isNull(); node = node.nextSibling()) {
    if (!node.isElement()) {
      continue;
    }
    if (node.nodeName() != pageTagName) {
      continue;
    }
    const QDomElement el(node.toElement());

    bool ok = true;
    const int id = el.attribute("id").toInt(&ok);
    if (!ok) {
      continue;
    }

    const PageId pageId(reader.pageId(id));
    if (pageId.isNull()) {
      continue;
    }

    const QDomElement resultEl(el.namedItem("ocr-result").toElement());
    if (resultEl.isNull()) {
      continue;
    }

    const OcrResult result(resultEl);
    m_settings->setOcrResult(pageId, result);
  }
}

void Filter::loadDefaultSettings(const PageInfo& pageInfo) {
  // No per-page default settings needed
}

std::shared_ptr<Task> Filter::createTask(const PageId& pageId,
                                         std::shared_ptr<output::Task> outputTask,
                                         const OutputFileNameGenerator& outFileNameGen,
                                         const bool batch) {
  return std::make_shared<Task>(std::static_pointer_cast<Filter>(shared_from_this()), std::move(outputTask), m_settings,
                                pageId, outFileNameGen, batch);
}

std::shared_ptr<CacheDrivenTask> Filter::createCacheDrivenTask(std::shared_ptr<output::CacheDrivenTask> outputTask,
                                                               const OutputFileNameGenerator& outFileNameGen,
                                                               std::shared_ptr<output::Settings> outputSettings) {
  return std::make_shared<CacheDrivenTask>(std::move(outputTask), m_settings, outFileNameGen, std::move(outputSettings));
}

OptionsWidget* Filter::optionsWidget() {
  return m_optionsWidget.get();
}
}  // namespace ocr
