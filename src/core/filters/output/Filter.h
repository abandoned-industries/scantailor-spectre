// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_OUTPUT_FILTER_H_
#define SCANTAILOR_OUTPUT_FILTER_H_

#include <QCoreApplication>
#include <QImage>
#include <memory>

#include "AbstractFilter.h"
#include "FillZonePropFactory.h"
#include "FilterResult.h"
#include "NonCopyable.h"
#include "PageView.h"
#include "PictureZonePropFactory.h"
#include "SafeDeletingQObjectPtr.h"

class PageSelectionAccessor;
class ProjectPages;
class ThumbnailPixmapCache;
class OutputFileNameGenerator;
class QString;

namespace finalize {
class Settings;
}

namespace output {
class OptionsWidget;
class Task;
class CacheDrivenTask;
class Settings;

class Filter : public AbstractFilter {
  DECLARE_NON_COPYABLE(Filter)

  Q_DECLARE_TR_FUNCTIONS(output::Filter)
 public:
  Filter(std::shared_ptr<ProjectPages> pages, const PageSelectionAccessor& pageSelectionAccessor);

  ~Filter() override;

  QString getName() const override;

  PageView getView() const override;

  void selected() override;

  void performRelinking(const AbstractRelinker& relinker) override;

  void preUpdateUI(FilterUiInterface* ui, const PageInfo& pageInfo) override;

  QDomElement saveSettings(const ProjectWriter& writer, QDomDocument& doc) const override;

  void loadSettings(const ProjectReader& reader, const QDomElement& filtersEl) override;

  void loadDefaultSettings(const PageInfo& pageInfo) override;

  std::shared_ptr<Task> createTask(const PageId& pageId,
                                   std::shared_ptr<ThumbnailPixmapCache> thumbnailCache,
                                   const OutputFileNameGenerator& outFileNameGen,
                                   bool batch,
                                   bool debug);

  std::shared_ptr<CacheDrivenTask> createCacheDrivenTask(const OutputFileNameGenerator& outFileNameGen);

  OptionsWidget* optionsWidget();

  std::shared_ptr<Settings> settings() const { return m_settings; }

  void setFinalizeSettings(std::shared_ptr<finalize::Settings> finalizeSettings);

  std::vector<PageOrderOption> pageOrderOptions() const override;

  int selectedPageOrder() const override;

  void selectPageOrder(int option) override;

 private:
  void writePageSettings(QDomDocument& doc, QDomElement& filterEl, const PageId& pageId, int numericId) const;

  std::shared_ptr<ProjectPages> m_pages;
  std::shared_ptr<Settings> m_settings;
  SafeDeletingQObjectPtr<OptionsWidget> m_optionsWidget;
  PictureZonePropFactory m_pictureZonePropFactory;
  FillZonePropFactory m_fillZonePropFactory;
  std::vector<PageOrderOption> m_pageOrderOptions;
  int m_selectedPageOrder;
};
}  // namespace output
#endif  // ifndef SCANTAILOR_OUTPUT_FILTER_H_
