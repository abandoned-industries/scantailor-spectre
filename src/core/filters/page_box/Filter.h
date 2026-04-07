// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_PAGE_BOX_FILTER_H_
#define SCANTAILOR_PAGE_BOX_FILTER_H_

#include <QCoreApplication>
#include <memory>
#include <vector>

#include "AbstractFilter.h"
#include "FilterResult.h"
#include "NonCopyable.h"
#include "PageOrderOption.h"
#include "PageView.h"
#include "SafeDeletingQObjectPtr.h"
#include "Settings.h"

class PageSelectionAccessor;
class QString;

namespace select_content {
class Task;
class CacheDrivenTask;
}  // namespace select_content

namespace page_box {
class OptionsWidget;
class Task;
class CacheDrivenTask;

class Filter : public AbstractFilter {
  DECLARE_NON_COPYABLE(Filter)
  Q_DECLARE_TR_FUNCTIONS(page_box::Filter)

 public:
  explicit Filter(const PageSelectionAccessor& pageSelectionAccessor);
  ~Filter() override;

  QString getName() const override;
  PageView getView() const override;
  int selectedPageOrder() const override;
  void selectPageOrder(int option) override;
  std::vector<PageOrderOption> pageOrderOptions() const override;
  void performRelinking(const AbstractRelinker& relinker) override;
  void preUpdateUI(FilterUiInterface* ui, const PageInfo& pageInfo) override;
  QDomElement saveSettings(const ProjectWriter& writer, QDomDocument& doc) const override;
  void loadSettings(const ProjectReader& reader, const QDomElement& filtersEl) override;
  void loadDefaultSettings(const PageInfo& pageInfo) override;

  std::shared_ptr<Task> createTask(const PageId& pageId,
                                   std::shared_ptr<select_content::Task> nextTask,
                                   bool batch,
                                   bool debug);

  std::shared_ptr<CacheDrivenTask> createCacheDrivenTask(
      std::shared_ptr<select_content::CacheDrivenTask> nextTask);

  OptionsWidget* optionsWidget();
  std::shared_ptr<Settings> settings() const { return m_settings; }

 private:
  void writePageSettings(QDomDocument& doc, QDomElement& filterEl, const PageId& pageId, int numericId) const;

  std::shared_ptr<Settings> m_settings;
  SafeDeletingQObjectPtr<OptionsWidget> m_optionsWidget;
  std::vector<PageOrderOption> m_pageOrderOptions;
  int m_selectedPageOrder;
};
}  // namespace page_box
#endif
