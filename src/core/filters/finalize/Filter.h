// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_FINALIZE_FILTER_H_
#define SCANTAILOR_FINALIZE_FILTER_H_

#include <QCoreApplication>
#include <memory>
#include <vector>

#include "AbstractFilter.h"
#include "FilterResult.h"
#include "NonCopyable.h"
#include "PageOrderOption.h"
#include "PageView.h"
#include "SafeDeletingQObjectPtr.h"

class ProjectPages;
class PageSelectionAccessor;
class QString;

namespace output {
class Task;
class CacheDrivenTask;
class Settings;
}  // namespace output

namespace finalize {
class OptionsWidget;
class Task;
class CacheDrivenTask;
class Settings;

class Filter : public AbstractFilter {
  DECLARE_NON_COPYABLE(Filter)

  Q_DECLARE_TR_FUNCTIONS(finalize::Filter)
 public:
  Filter(std::shared_ptr<ProjectPages> pageSequence, const PageSelectionAccessor& pageSelectionAccessor);

  ~Filter() override;

  QString getName() const override;

  PageView getView() const override;

  void selected() override;

  int selectedPageOrder() const override;

  void selectPageOrder(int option) override;

  std::vector<PageOrderOption> pageOrderOptions() const override;

  void performRelinking(const AbstractRelinker& relinker) override;

  void preUpdateUI(FilterUiInterface* ui, const PageInfo& pageInfo) override;

  QDomElement saveSettings(const ProjectWriter& writer, QDomDocument& doc) const override;

  void loadSettings(const ProjectReader& reader, const QDomElement& filtersEl) override;

  void loadDefaultSettings(const PageInfo& pageInfo) override;

  bool checkReadyForOutput(const ProjectPages& pages, const PageId* ignore = nullptr);

  std::shared_ptr<Task> createTask(const PageId& pageId,
                                   std::shared_ptr<output::Task> nextTask,
                                   std::shared_ptr<output::Settings> outputSettings,
                                   bool batch);

  std::shared_ptr<CacheDrivenTask> createCacheDrivenTask(std::shared_ptr<output::CacheDrivenTask> nextTask);

  OptionsWidget* optionsWidget();

  std::shared_ptr<Settings> settings() const { return m_settings; }

  void setOutputSettings(std::shared_ptr<output::Settings> outputSettings);

 private:
  void writePageSettings(QDomDocument& doc, QDomElement& filterEl, const PageId& pageId, int numericId) const;

  std::shared_ptr<ProjectPages> m_pages;
  std::shared_ptr<Settings> m_settings;
  SafeDeletingQObjectPtr<OptionsWidget> m_optionsWidget;
  std::vector<PageOrderOption> m_pageOrderOptions;
  int m_selectedPageOrder;
};
}  // namespace finalize
#endif  // SCANTAILOR_FINALIZE_FILTER_H_
