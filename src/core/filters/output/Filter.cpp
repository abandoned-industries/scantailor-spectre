// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "Filter.h"

#include <OrderByCompletenessProvider.h>
#include <QDir>
#include <QFileInfo>

#include "CacheDrivenTask.h"
#include "FilterUiInterface.h"
#include "OptionsWidget.h"
#include "OutputFileNameGenerator.h"
#include "OutputFileParams.h"
#include "OutputParams.h"
#include "Params.h"
#include "ProjectReader.h"
#include "ProjectWriter.h"
#include "RenderParams.h"
#include "Settings.h"
#include "Task.h"
#include "ThumbnailPixmapCache.h"
#include "Utils.h"

namespace output {
Filter::Filter(std::shared_ptr<ProjectPages>, const PageSelectionAccessor& pageSelectionAccessor)
    : m_settings(std::make_shared<Settings>()), m_selectedPageOrder(0) {
  m_optionsWidget.reset(new OptionsWidget(m_settings, pageSelectionAccessor));

  const PageOrderOption::ProviderPtr defaultOrder;
  const auto orderByCompleteness = std::make_shared<OrderByCompletenessProvider>();
  m_pageOrderOptions.emplace_back(tr("Natural order"), defaultOrder);
  m_pageOrderOptions.emplace_back(tr("Order by completeness"), orderByCompleteness);
}

Filter::~Filter() = default;

QString Filter::getName() const {
  return QCoreApplication::translate("output::Filter", "Output");
}

PageView Filter::getView() const {
  return PAGE_VIEW;
}

bool Filter::isPageComplete(const PageId& pageId) const {
  if (!m_outFileNameGen) {
    return false;
  }

  const std::unique_ptr<OutputParams> storedOutputParams(m_settings->getOutputParams(pageId));
  if (!storedOutputParams) {
    return false;
  }

  const QFileInfo sourceFileInfo(pageId.imageId().filePath());
  if (!storedOutputParams->sourceFileParams().matches(OutputFileParams(sourceFileInfo))) {
    return false;
  }

  const QString outFilePath(m_outFileNameGen->filePathFor(pageId));
  const QFileInfo outFileInfo(outFilePath);
  const RenderParams renderParams(m_settings->getParams(pageId).colorParams(),
                                  m_settings->getParams(pageId).splittingOptions());
  if (!renderParams.splitOutput()) {
    return outFileInfo.exists()
        && storedOutputParams->outputFileParams().matches(OutputFileParams(outFileInfo));
  }

  const QFileInfo foregroundFileInfo(
      QDir(Utils::foregroundDir(m_outFileNameGen->outDir())).absoluteFilePath(outFileInfo.fileName()));
  const QFileInfo backgroundFileInfo(
      QDir(Utils::backgroundDir(m_outFileNameGen->outDir())).absoluteFilePath(outFileInfo.fileName()));
  if (!foregroundFileInfo.exists() || !backgroundFileInfo.exists()
      || !storedOutputParams->foregroundFileParams().matches(OutputFileParams(foregroundFileInfo))
      || !storedOutputParams->backgroundFileParams().matches(OutputFileParams(backgroundFileInfo))) {
    return false;
  }

  if (!renderParams.originalBackground()) {
    return true;
  }
  const QFileInfo originalBackgroundFileInfo(
      QDir(Utils::originalBackgroundDir(m_outFileNameGen->outDir())).absoluteFilePath(outFileInfo.fileName()));
  return originalBackgroundFileInfo.exists()
      && storedOutputParams->originalBackgroundFileParams().matches(OutputFileParams(originalBackgroundFileInfo));
}

void Filter::performRelinking(const AbstractRelinker& relinker) {
  m_settings->performRelinking(relinker);
}

void Filter::preUpdateUI(FilterUiInterface* ui, const PageInfo& pageInfo) {
  m_optionsWidget->preUpdateUI(pageInfo.id());
  ui->setOptionsWidget(m_optionsWidget.get(), ui->KEEP_OWNERSHIP);
}

QDomElement Filter::saveSettings(const ProjectWriter& writer, QDomDocument& doc) const {
  QDomElement filterEl(doc.createElement("output"));

  writer.enumPages(
      [&](const PageId& pageId, int numericId) { this->writePageSettings(doc, filterEl, pageId, numericId); });
  return filterEl;
}

void Filter::writePageSettings(QDomDocument& doc, QDomElement& filterEl, const PageId& pageId, int numericId) const {
  const Params params(m_settings->getParams(pageId));

  QDomElement pageEl(doc.createElement("page"));
  pageEl.setAttribute("id", numericId);

  pageEl.appendChild(m_settings->pictureZonesForPage(pageId).toXml(doc, "zones"));
  pageEl.appendChild(m_settings->fillZonesForPage(pageId).toXml(doc, "fill-zones"));
  pageEl.appendChild(params.toXml(doc, "params"));
  pageEl.appendChild(m_settings->getOutputProcessingParams(pageId).toXml(doc, "processing-params"));

  std::unique_ptr<OutputParams> outputParams(m_settings->getOutputParams(pageId));
  if (outputParams) {
    pageEl.appendChild(outputParams->toXml(doc, "output-params"));
  }

  filterEl.appendChild(pageEl);
}

void Filter::loadSettings(const ProjectReader& reader, const QDomElement& filtersEl) {
  m_settings->clear();

  const QDomElement filterEl(filtersEl.namedItem("output").toElement());

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

    const ZoneSet pictureZones(el.namedItem("zones").toElement(), m_pictureZonePropFactory);
    if (!pictureZones.empty()) {
      m_settings->setPictureZones(pageId, pictureZones);
    }

    const ZoneSet fillZones(el.namedItem("fill-zones").toElement(), m_fillZonePropFactory);
    if (!fillZones.empty()) {
      m_settings->setFillZones(pageId, fillZones);
    }

    const QDomElement paramsEl(el.namedItem("params").toElement());
    if (!paramsEl.isNull()) {
      const Params params(paramsEl);
      m_settings->setParams(pageId, params);
    }

    const QDomElement outputProcessingParamsEl(el.namedItem("processing-params").toElement());
    if (!outputProcessingParamsEl.isNull()) {
      const OutputProcessingParams outputProcessingParams(outputProcessingParamsEl);
      m_settings->setOutputProcessingParams(pageId, outputProcessingParams);
    }

    const QDomElement outputParamsEl(el.namedItem("output-params").toElement());
    if (!outputParamsEl.isNull()) {
      const OutputParams outputParams(outputParamsEl);
      m_settings->setOutputParams(pageId, outputParams);
    }
  }
}  // Filter::loadSettings

std::shared_ptr<Task> Filter::createTask(const PageId& pageId,
                                         std::shared_ptr<ThumbnailPixmapCache> thumbnailCache,
                                         const OutputFileNameGenerator& outFileNameGen,
                                         const bool batch,
                                         const bool debug) {
  ImageViewTab lastTab(TAB_OUTPUT);
  if (m_optionsWidget.get() != nullptr) {
    lastTab = m_optionsWidget->lastTab();
  }
  return std::make_shared<Task>(std::static_pointer_cast<Filter>(shared_from_this()), m_settings,
                                std::move(thumbnailCache), pageId, outFileNameGen, lastTab, batch, debug);
}

std::shared_ptr<CacheDrivenTask> Filter::createCacheDrivenTask(const OutputFileNameGenerator& outFileNameGen) {
  m_outFileNameGen = std::make_unique<OutputFileNameGenerator>(outFileNameGen);
  return std::make_shared<CacheDrivenTask>(m_settings, outFileNameGen);
}

void Filter::loadDefaultSettings(const PageInfo& pageInfo) {
  // Keep missing params absent until Finalize runs. Settings::getParams()
  // supplies the existing safe color-mode/default-DPI values meanwhile.
}

OptionsWidget* Filter::optionsWidget() {
  return m_optionsWidget.get();
}

void Filter::setDefaultDpi(const Dpi& dpi) {
  m_settings->setDefaultDpi(dpi);
}

void Filter::setFinalizeSettings(std::shared_ptr<finalize::Settings> finalizeSettings) {
  m_optionsWidget->setFinalizeSettings(std::move(finalizeSettings));
}

std::vector<PageOrderOption> Filter::pageOrderOptions() const {
  return m_pageOrderOptions;
}

int Filter::selectedPageOrder() const {
  return m_selectedPageOrder;
}

void Filter::selectPageOrder(int option) {
  assert((unsigned) option < m_pageOrderOptions.size());
  m_selectedPageOrder = option;
}
}  // namespace output
