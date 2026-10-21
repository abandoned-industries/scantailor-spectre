// Copyright (C) 2026 ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include <QApplication>
#include <QCoreApplication>
#include <QDockWidget>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLayout>
#include <QMainWindow>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedLayout>
#include <QVBoxLayout>
#include <QWidget>

#include <array>
#include <cstdio>
#include <memory>

#include "NonOwningWidget.h"
#include "PageSelectionAccessor.h"
#include "ProjectPages.h"
#include "StageActionPanel.h"
#include "StageColumnLayout.h"
#include "StageSequence.h"
#include "ThumbnailSequence.h"
#include "filters/export/OptionsWidget.h"

namespace {

constexpr int kBaseDockMinimumWidth = 274;

struct StageCase {
  const char* name;
  QWidget* options;
  export_::OptionsWidget* exportOptions = nullptr;
};

int fail(const char* stage, const char* message) {
  std::fprintf(stderr, "Stage column layout regression [%s]: %s\n", stage, message);
  return 1;
}

bool visibleOptionsChildExceedsViewport(QWidget* options,
                                        QScrollArea* scrollArea,
                                        QWidget** offender = nullptr) {
  const int viewportWidth = scrollArea->viewport()->width();
  const QList<QWidget*> children = options->findChildren<QWidget*>();
  for (QWidget* child : children) {
    if (!child->isVisible() || child->isWindow()) {
      continue;
    }
    if (child->mapTo(scrollArea->viewport(), child->rect().bottomRight()).x()
        > viewportWidth) {
      if (offender) {
        *offender = child;
      }
      return true;
    }
  }
  return false;
}

void clearLayout(QLayout* layout) {
  while (QLayoutItem* item = layout->takeAt(0)) {
    delete item;
  }
}

int testThumbnailPageScroll() {
  constexpr int rowHeight = 80;
  QGraphicsScene scene;
  for (int row = 0; row < 30; ++row) {
    scene.addRect(0, row * rowHeight, 100, rowHeight - 3);
  }

  QGraphicsView view(&scene);
  view.resize(180, 260);
  view.show();
  QApplication::processEvents();

  QScrollBar* const scrollBar = view.verticalScrollBar();
  const int viewportHeight = view.viewport()->height();
  const int expectedStep = viewportHeight - rowHeight;
  const int middle = scrollBar->maximum() / 2;

  const int down = ThumbnailScroll::pageTarget(middle, scrollBar->minimum(), scrollBar->maximum(),
                                                viewportHeight, rowHeight, scrollBar->pageStep(), 1);
  if (down - middle != expectedStep) {
    return fail("Thumbnail page scroll", "Page Down does not leave exactly one row of overlap");
  }
  const int up = ThumbnailScroll::pageTarget(middle, scrollBar->minimum(), scrollBar->maximum(),
                                              viewportHeight, rowHeight, scrollBar->pageStep(), -1);
  if (middle - up != expectedStep) {
    return fail("Thumbnail page scroll", "Page Up does not leave exactly one row of overlap");
  }
  if (ThumbnailScroll::pageTarget(scrollBar->minimum(), scrollBar->minimum(), scrollBar->maximum(),
                                  viewportHeight, rowHeight, scrollBar->pageStep(), -1)
      != scrollBar->minimum()) {
    return fail("Thumbnail page scroll", "Page Up does not clamp at the top");
  }
  if (ThumbnailScroll::pageTarget(scrollBar->maximum(), scrollBar->minimum(), scrollBar->maximum(),
                                  viewportHeight, rowHeight, scrollBar->pageStep(), 1)
      != scrollBar->maximum()) {
    return fail("Thumbnail page scroll", "Page Down does not clamp at the bottom");
  }
  if (ThumbnailScroll::pageTarget(middle, scrollBar->minimum(), scrollBar->maximum(),
                                  rowHeight - 1, rowHeight, scrollBar->pageStep(), 1)
      != middle) {
    return fail("Thumbnail page scroll", "a viewport shorter than one row scrolls backwards");
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  // Use non-overlay scrollbars for the old missing-scrollbar-width witness.
  // Native macOS styles vary across Qt versions and user scrollbar settings.
  app.setStyle(QStringLiteral("Fusion"));
  // Exercise a readable larger font so the 274px base floor cannot hide the
  // omitted scrollbar allowance on Qt versions with narrower native metrics.
  QFont testFont = app.font();
  testFont.setPointSize(16);
  app.setFont(testFont);

  if (const int result = testThumbnailPageScroll()) {
    return result;
  }

  const auto pages = std::make_shared<ProjectPages>();
  const PageSelectionAccessor pageSelectionAccessor{
      std::shared_ptr<const PageSelectionProvider>()};
  auto stages = std::make_unique<StageSequence>(pages, pageSelectionAccessor);

  const std::array<StageCase, 10> stageCases{{
      {"Fix Orientation", reinterpret_cast<QWidget*>(stages->fixOrientationFilter()->optionsWidget())},
      {"Split Pages", reinterpret_cast<QWidget*>(stages->pageSplitFilter()->optionsWidget())},
      {"Deskew", reinterpret_cast<QWidget*>(stages->deskewFilter()->optionsWidget())},
      {"Page Box", reinterpret_cast<QWidget*>(stages->pageBoxFilter()->optionsWidget())},
      {"Select Content", reinterpret_cast<QWidget*>(stages->selectContentFilter()->optionsWidget())},
      {"Margins", reinterpret_cast<QWidget*>(stages->pageLayoutFilter()->optionsWidget())},
      {"Finalize", reinterpret_cast<QWidget*>(stages->finalizeFilter()->optionsWidget())},
      {"Output", reinterpret_cast<QWidget*>(stages->outputFilter()->optionsWidget())},
      {"OCR", reinterpret_cast<QWidget*>(stages->ocrFilter()->optionsWidget())},
      {"Export", stages->exportFilter()->optionsWidget(), stages->exportFilter()->optionsWidget()},
  }};

  QMainWindow window;
  window.setCentralWidget(new QWidget(&window));
  auto* dock = new QDockWidget(&window);
  dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
  auto* scrollArea = new QScrollArea(dock);
  scrollArea->setWidgetResizable(true);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  auto* contents = new QWidget;
  auto* contentsLayout = new QVBoxLayout(contents);
  contentsLayout->setContentsMargins(0, 0, 0, 0);
  contentsLayout->setSpacing(0);
  auto* filterOptions = new NonOwningWidget(contents);
  auto* optionsLayout = new QStackedLayout(filterOptions);
  auto* panel = new StageActionPanel(filterOptions, contents);
  contentsLayout->addWidget(panel);
  scrollArea->setWidget(contents);
  dock->setWidget(scrollArea);
  window.addDockWidget(Qt::LeftDockWidgetArea, dock);
  window.resize(800, 600);
  window.show();
  QApplication::processEvents();

  auto showStage = [&](const StageCase& stage) {
    clearLayout(optionsLayout);
    optionsLayout->addWidget(stage.options);
    panel->setHeadingText(stage.exportOptions ? QStringLiteral("Your book is ready.") : QString());
    panel->setActionRowVisible(stage.exportOptions == nullptr);
    panel->setPinnedWidget(stage.exportOptions ? stage.exportOptions->exportFooterWidget() : nullptr);
    panel->setAdjustOpen(true);
    stage.options->show();
    filterOptions->show();
    panel->show();
    if (optionsLayout->activate()) {
      filterOptions->updateGeometry();
    }
    panel->layout()->activate();
    contentsLayout->activate();
    QApplication::processEvents();
  };

  const StageCase& finalize = stageCases[6];
  showStage(finalize);
  scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  QApplication::processEvents();
  const int oldDockChrome = qMax(0, dock->width() - scrollArea->viewport()->width());
  const int oldContentWidth = qMax(finalize.options->minimumSizeHint().width(),
                                   panel->minimumContentWidth());
  const int oldRequiredWidth = qMax(kBaseDockMinimumWidth,
                                    oldContentWidth + oldDockChrome);
  dock->setMinimumWidth(oldRequiredWidth);
  dock->setMaximumWidth(oldRequiredWidth);
  window.resizeDocks({dock}, {oldRequiredWidth}, Qt::Horizontal);
  QApplication::processEvents();
  // The old calculation ran before the as-needed bar appeared and therefore
  // reserved no width for it. Force that exact transition deterministically.
  scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  QApplication::processEvents();
  QWidget* oldOffender = nullptr;
  std::fprintf(stderr, "OLD_GEOMETRY required=%d dock=%d viewport=%d contents=%d optionsMin=%d panelMin=%d bar=%d\n",
      oldRequiredWidth, dock->width(), scrollArea->viewport()->width(), contents->width(),
      finalize.options->minimumSizeHint().width(), panel->minimumContentWidth(), scrollArea->verticalScrollBar()->width());
  if (!visibleOptionsChildExceedsViewport(finalize.options, scrollArea, &oldOffender)) {
    return fail(finalize.name, "the documented old computation did not reproduce scrollbar overflow");
  }
  std::fprintf(stderr, "OLD Finalize %d px overflow: %s right=%d viewport=%d\n",
               oldRequiredWidth,
               oldOffender->objectName().toUtf8().constData(),
               oldOffender->mapTo(scrollArea->viewport(), oldOffender->rect().bottomRight()).x(),
               scrollArea->viewport()->width());
  dock->setMaximumWidth(QWIDGETSIZE_MAX);
  scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  for (const StageCase& stage : stageCases) {
    showStage(stage);
    dock->setMinimumWidth(kBaseDockMinimumWidth);
    window.resizeDocks({dock}, {kBaseDockMinimumWidth}, Qt::Horizontal);
    QApplication::processEvents();

    const int requiredWidth = StageColumnLayout::apply(
        &window, dock, scrollArea, contents, panel, kBaseDockMinimumWidth);
    QApplication::processEvents();
    std::fprintf(stderr, "STAGE_WIDTH %s %d\n", stage.name, requiredWidth);

    if (contents->width() > scrollArea->viewport()->width()) {
      std::fprintf(stderr, "content=%d viewport=%d dock=%d required=%d\n",
                   contents->width(), scrollArea->viewport()->width(),
                   dock->width(), requiredWidth);
      return fail(stage.name, "scroll contents are wider than the viewport");
    }

    const QRect panelRect = panel->contentsRect();
    for (const char* buttonName : {"primaryActionButton", "secondaryActionButton"}) {
      auto* button = panel->findChild<QPushButton*>(QString::fromLatin1(buttonName));
      if (!button) {
        return fail(stage.name, "an action button is missing");
      }
      const int right = button->mapTo(panel, button->rect().bottomRight()).x();
      if (button->isVisible() && right > panelRect.right()) {
        std::fprintf(stderr, "%s right=%d panel=%d\n", buttonName, right, panelRect.right());
        return fail(stage.name, "an action button exceeds the panel contents rect");
      }
    }

    QWidget* offender = nullptr;
    if (visibleOptionsChildExceedsViewport(stage.options, scrollArea, &offender)) {
      std::fprintf(stderr, "%s right=%d viewport=%d\n",
                   offender->objectName().toUtf8().constData(),
                   offender->mapTo(scrollArea->viewport(), offender->rect().bottomRight()).x(),
                   scrollArea->viewport()->width());
      return fail(stage.name, "a visible OptionsWidget child exceeds the viewport");
    }
  }

  panel->setPinnedWidget(nullptr);
  stageCases.back().exportOptions->restoreExportFooter();
  clearLayout(optionsLayout);
  stages.reset();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  return 0;
}
