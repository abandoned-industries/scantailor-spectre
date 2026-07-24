// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "Task.h"

#include <utility>

#include "DebugImagesImpl.h"
#include "Filter.h"
#include "FilterData.h"
#include "FilterUiInterface.h"
#include "OptionsWidget.h"
#include "PageFinder.h"
#include "Settings.h"
#include "TaskStatus.h"
#include "Thumbnail.h"
#include "ImageView.h"
#include "filters/select_content/Task.h"

namespace page_box {
class Task::UiUpdater : public FilterResult {
 public:
  UiUpdater(std::shared_ptr<Filter> filter,
            const PageId& pageId,
            std::unique_ptr<DebugImages> dbg,
            const QImage& image,
            const ImageTransformation& xform,
            const QRectF& pageRect,
            AutoManualMode pageDetectionMode,
            bool fineTuneCorners,
            bool batch);

  void updateUI(FilterUiInterface* ui) override;
  std::shared_ptr<AbstractFilter> filter() override { return m_filter; }

 private:
  std::shared_ptr<Filter> m_filter;
  PageId m_pageId;
  std::unique_ptr<DebugImages> m_dbg;
  QImage m_image;
  QImage m_downscaledImage;
  ImageTransformation m_xform;
  QRectF m_pageRect;
  AutoManualMode m_pageDetectionMode;
  bool m_fineTuneCorners;
  bool m_batchProcessing;
};


Task::Task(std::shared_ptr<Filter> filter,
           std::shared_ptr<select_content::Task> nextTask,
           std::shared_ptr<Settings> settings,
           const PageId& pageId,
           const bool batch,
           const bool debug)
    : m_filter(std::move(filter)),
      m_nextTask(std::move(nextTask)),
      m_settings(std::move(settings)),
      m_pageId(pageId),
      m_batchProcessing(batch) {
  if (debug) {
    m_dbg = std::make_unique<DebugImagesImpl>();
  }
}

Task::~Task() = default;

FilterResultPtr Task::process(const TaskStatus& status, const FilterData& data) {
  status.throwIfCancelled();

  std::unique_ptr<Params> params(m_settings->getPageParams(m_pageId));
  const Dependencies deps = params
      ? Dependencies(data.xform().resultingPreCropArea(), params->pageDetectionMode(), params->isFineTuningEnabled())
      : Dependencies(data.xform().resultingPreCropArea());

  Params newParams(deps);
  if (params) {
    newParams = *params;
    newParams.setDependencies(deps);
  }

  bool needUpdate = false;
  if (!params || !deps.compatibleWith(params->dependencies(), &needUpdate)) {
    QRectF pageRect(newParams.pageRect());

    if (needUpdate) {
      if (newParams.pageDetectionMode() == MODE_AUTO) {
        pageRect = PageFinder::findPageBox(status, data, newParams.isFineTuningEnabled(),
                                           m_settings->pageDetectionBox(),
                                           m_settings->pageDetectionTolerance(), m_dbg.get());
      } else if (newParams.pageDetectionMode() == MODE_DISABLED) {
        pageRect = data.xform().resultingRect();
      }

      if (!data.xform().resultingRect().intersected(pageRect).isValid()) {
        pageRect = data.xform().resultingRect();
      }

      newParams.setPageRect(pageRect);
    }
  }

  // If page rect is not set yet, use the full image
  if (newParams.pageRect().isNull()) {
    newParams.setPageRect(data.xform().resultingRect());
  }

  m_settings->setPageParams(m_pageId, newParams);

  status.throwIfCancelled();

  if (m_nextTask) {
    return m_nextTask->process(status, data);
  } else {
    return std::make_shared<UiUpdater>(m_filter, m_pageId, std::move(m_dbg), data.origImage(), data.xform(),
                                       newParams.pageRect(), newParams.pageDetectionMode(),
                                       newParams.isFineTuningEnabled(), m_batchProcessing);
  }
}


/*============================ Task::UiUpdater ==========================*/

Task::UiUpdater::UiUpdater(std::shared_ptr<Filter> filter,
                           const PageId& pageId,
                           std::unique_ptr<DebugImages> dbg,
                           const QImage& image,
                           const ImageTransformation& xform,
                           const QRectF& pageRect,
                           AutoManualMode pageDetectionMode,
                           bool fineTuneCorners,
                           const bool batch)
    : m_filter(std::move(filter)),
      m_pageId(pageId),
      m_dbg(std::move(dbg)),
      m_image(image),
      m_xform(xform),
      m_pageRect(pageRect),
      m_pageDetectionMode(pageDetectionMode),
      m_fineTuneCorners(fineTuneCorners),
      m_batchProcessing(batch) {}

void Task::UiUpdater::updateUI(FilterUiInterface* ui) {
  ui->invalidateThumbnail(m_pageId);

  if (m_batchProcessing) {
    return;
  }

  OptionsWidget* const optWidget = m_filter->optionsWidget();
  optWidget->postUpdateUI(m_pageRect, m_pageDetectionMode, m_fineTuneCorners);
  ui->setOptionsWidget(optWidget, ui->KEEP_OWNERSHIP);

  auto* view = new ImageView(m_image, ImageView::createDownscaledImage(m_image), m_xform, m_pageRect);
  ui->setImageWidget(view, ui->TRANSFER_OWNERSHIP, m_dbg.get());

  QObject::connect(view, SIGNAL(manualPageRectSet(const QRectF&)), optWidget, SLOT(manualPageRectSet(const QRectF&)));
  QObject::connect(view, SIGNAL(pageRectSizeChanged(const QSizeF&)), optWidget, SLOT(updatePageRectSize(const QSizeF&)));
  QObject::connect(optWidget, SIGNAL(pageRectChangedLocally(const QRectF&)), view, SLOT(pageRectSetExternally(const QRectF&)));
}
}  // namespace page_box
