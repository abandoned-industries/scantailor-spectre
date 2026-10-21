// Copyright (C) 2026 ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include <QApplication>
#include <QCoreApplication>
#include <QDockWidget>
#include <QEvent>
#include <QLayout>
#include <QMainWindow>
#include <QObject>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedLayout>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QtGlobal>

#include <array>
#include <cstdio>
#include <memory>
#include <vector>

#include "ExportPanelHeightLayout.h"
#include "NonOwningWidget.h"
#include "PageSelectionAccessor.h"
#include "ProjectPages.h"
#include "StageActionPanel.h"
#include "StageColumnLayout.h"
#include "StageSequence.h"
#include "filters/export/OptionsWidget.h"

namespace {

constexpr int kBaseDockMinimumWidth = 274;

struct StageCase {
  const char* name;
  QWidget* options;
  export_::OptionsWidget* exportOptions = nullptr;
};

int fail(const char* stage, const char* message) {
  std::fprintf(stderr, "Export panel oscillation regression [%s]: %s\n", stage, message);
  return 1;
}

void clearLayout(QLayout* layout) {
  while (QLayoutItem* item = layout->takeAt(0)) {
    delete item;
  }
}

// Mirrors the Export-relevant branch of MainWindow::eventFilter(): a resize of
// the outer scroll area's viewport re-runs the fixed-height computation (for
// Export only) synchronously, and the dock-width computation debounced with
// the same QTimer::singleShot(0, ...) pattern production uses. This is
// exactly the mechanism that can drive a resize -> relayout -> resize
// feedback loop if either computation is sensitive to an input that its own
// output changes (e.g. the outer vertical scrollbar's current visibility).
class ResizeRelayout : public QObject {
 public:
  ResizeRelayout(QMainWindow* window, QDockWidget* dock, QScrollArea* scrollArea, QWidget* contents,
                 StageActionPanel* panel, QWidget* filterOptions)
      : m_window(window),
        m_dock(dock),
        m_scrollArea(scrollArea),
        m_contents(contents),
        m_panel(panel),
        m_filterOptions(filterOptions) {}

  void setExportOptions(export_::OptionsWidget* exportOptions) { m_exportOptions = exportOptions; }

 protected:
  bool eventFilter(QObject* obj, QEvent* event) override {
    if ((obj == m_scrollArea->viewport()) && (event->type() == QEvent::Resize)) {
      if (m_exportOptions) {
        ExportPanelHeightLayout::apply(m_panel, m_filterOptions, m_exportOptions->minimumSizeHint().height(),
                                       m_scrollArea);
      }
      if (!m_widthUpdatePending) {
        m_widthUpdatePending = true;
        QTimer::singleShot(0, this, [this]() {
          m_widthUpdatePending = false;
          StageColumnLayout::apply(m_window, m_dock, m_scrollArea, m_contents, m_panel,
                                   kBaseDockMinimumWidth);
        });
      }
    }
    return false;
  }

 private:
  QMainWindow* m_window;
  QDockWidget* m_dock;
  QScrollArea* m_scrollArea;
  QWidget* m_contents;
  StageActionPanel* m_panel;
  QWidget* m_filterOptions;
  export_::OptionsWidget* m_exportOptions = nullptr;
  bool m_widthUpdatePending = false;
};

struct Snapshot {
  int filterOptionsHeight;
  int contentsHeight;
  int dockWidth;
  int panelWidth;
  bool vScrollVisible;

  bool operator!=(const Snapshot& other) const {
    return filterOptionsHeight != other.filterOptionsHeight || contentsHeight != other.contentsHeight
           || dockWidth != other.dockWidth || panelWidth != other.panelWidth
           || vScrollVisible != other.vScrollVisible;
  }
};

Snapshot takeSnapshot(QWidget* filterOptions, QWidget* contents, QDockWidget* dock, QWidget* panel,
                      QScrollArea* scrollArea) {
  return {filterOptions->height(), contents->height(), dock->width(), panel->width(),
          scrollArea->verticalScrollBar()->isVisible()};
}

// Pumps the event loop `totalIterations` times, recording a geometry
// snapshot after each pump. Returns true (and prints a diagnostic history)
// if the layout is still changing during the last `settleIterations` pumps
// -- i.e. it never reaches a fixed point, which is what a continuous visible
// "vibration" looks like offscreen. A one-shot settle (a handful of changing
// iterations, then stable) is expected and NOT a failure.
bool oscillates(const char* stageName, const char* sizeLabel, QWidget* filterOptions, QWidget* contents,
                QDockWidget* dock, QWidget* panel, QScrollArea* scrollArea, const int totalIterations,
                const int settleIterations) {
  std::vector<Snapshot> history;
  history.reserve(totalIterations);
  for (int i = 0; i < totalIterations; ++i) {
    QApplication::processEvents();
    QApplication::sendPostedEvents();
    history.push_back(takeSnapshot(filterOptions, contents, dock, panel, scrollArea));
  }

  const int histSize = static_cast<int>(history.size());
  bool stableAtEnd = true;
  for (int i = qMax(1, histSize - settleIterations); i < histSize; ++i) {
    if (history[i] != history[i - 1]) {
      stableAtEnd = false;
      break;
    }
  }

  if (!stableAtEnd) {
    std::fprintf(stderr, "%s @ %s: layout did not settle over %d iterations (checked last %d):\n", stageName,
                 sizeLabel, totalIterations, settleIterations);
    const int start = qMax(0, histSize - settleIterations - 3);
    for (int i = start; i < histSize; ++i) {
      const Snapshot& s = history[i];
      std::fprintf(stderr, "  [%2d] filterOptions=%4d contents=%4d dock=%4d panel=%4d vScroll=%d\n", i,
                   s.filterOptionsHeight, s.contentsHeight, s.dockWidth, s.panelWidth, s.vScrollVisible);
    }
  }
  return !stableAtEnd;
}

}  // namespace

int main(int argc, char** argv) {
  QApplication app(argc, argv);

  const auto pages = std::make_shared<ProjectPages>();
  const PageSelectionAccessor pageSelectionAccessor{std::shared_ptr<const PageSelectionProvider>()};
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
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  // Deliberately NOT overriding the vertical policy here: MainWindow.ui
  // leaves it at Qt's default (AsNeeded), which is what lets the outer
  // scroll area's vertical scrollbar toggle in the first place -- that
  // toggle is the width-changing event this test is trying to provoke.
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

  ResizeRelayout relayout(&window, dock, scrollArea, contents, panel, filterOptions);
  scrollArea->viewport()->installEventFilter(&relayout);

  auto showStage = [&](const StageCase& stage) {
    relayout.setExportOptions(nullptr);
    clearLayout(optionsLayout);
    optionsLayout->addWidget(stage.options);
    panel->setHeadingText(stage.exportOptions ? QStringLiteral("Your book is ready.") : QString());
    panel->setActionRowVisible(stage.exportOptions == nullptr);
    panel->setPinnedWidget(stage.exportOptions ? stage.exportOptions->exportFooterWidget() : nullptr);
    panel->setAdjustOpen(true);
    stage.options->show();
    filterOptions->show();
    panel->show();
    // Mirrors MainWindow::setOptionsWidget(): reset any previous fixed
    // height before the fresh layout pass below.
    filterOptions->setMinimumHeight(0);
    filterOptions->setMaximumHeight(QWIDGETSIZE_MAX);
    if (optionsLayout->activate()) {
      filterOptions->updateGeometry();
    }
    panel->layout()->activate();
    contentsLayout->activate();
    QApplication::processEvents();
    // Mirrors setOptionsWidget()'s initial, resize-independent seeding of
    // the dock width and (for Export) the fixed options height.
    StageColumnLayout::apply(&window, dock, scrollArea, contents, panel, kBaseDockMinimumWidth);
    if (stage.exportOptions) {
      ExportPanelHeightLayout::apply(panel, filterOptions, stage.exportOptions->minimumSizeHint().height(),
                                     scrollArea);
    }
    relayout.setExportOptions(stage.exportOptions);
    QApplication::processEvents();
  };

  const std::array<std::pair<const char*, QSize>, 2> windowSizes{{
      {"1400x900 (realistic)", QSize(1400, 900)},
      {"1400x560 (short, forces vertical scrollbar)", QSize(1400, 560)},
  }};

  int failureCount = 0;
  for (const auto& [sizeLabel, size] : windowSizes) {
    window.resize(size);
    window.show();
    QApplication::processEvents();

    for (const StageCase& stage : stageCases) {
      showStage(stage);
      if (oscillates(stage.name, sizeLabel, filterOptions, contents, dock, panel, scrollArea,
                     /*totalIterations=*/60, /*settleIterations=*/15)) {
        ++failureCount;
      }
    }
  }

  panel->setPinnedWidget(nullptr);
  stageCases.back().exportOptions->restoreExportFooter();
  clearLayout(optionsLayout);
  stages.reset();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

  if (failureCount > 0) {
    return fail("summary", "one or more stage/size combinations never reached a stable layout");
  }
  return 0;
}
