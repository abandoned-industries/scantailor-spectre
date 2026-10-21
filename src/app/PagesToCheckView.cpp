// Copyright (C) 2026  ScanTailor Spectre contributors.
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "PagesToCheckView.h"

#include <QAction>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

#include "ThumbnailPixmapCache.h"

namespace {

constexpr int kCardWidth = 272;
constexpr int kGridGap = 16;
constexpr int kGridMargin = 24;

bool containsPage(const std::vector<PageId>& pages, const PageId& pageId) {
  return std::find(pages.begin(), pages.end(), pageId) != pages.end();
}

}  // namespace

PagesToCheckView::PagesToCheckView(const ReviewModel& model,
                                   std::shared_ptr<ThumbnailPixmapCache> thumbnailCache,
                                   QWidget* parent)
    : QScrollArea(parent),
      m_model(model),
      m_thumbnailCache(std::move(thumbnailCache)),
      m_cardsWidget(new QWidget(this)),
      m_grid(new QGridLayout(m_cardsWidget)) {
  setObjectName(QStringLiteral("pagesToCheckView"));
  setWidgetResizable(true);
  setFrameShape(QFrame::NoFrame);
  m_cardsWidget->setObjectName(QStringLiteral("reviewCardsWell"));
  m_grid->setContentsMargins(kGridMargin, kGridMargin, kGridMargin, kGridMargin);
  m_grid->setHorizontalSpacing(kGridGap);
  m_grid->setVerticalSpacing(kGridGap);
  m_grid->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
  setWidget(m_cardsWidget);
  rebuildCards();
}

void PagesToCheckView::setModel(const ReviewModel& model) {
  m_model = model;
  rebuildCards();
  emit remainingChanged(static_cast<int>(listedPageIds().size()));
}

std::vector<PageId> PagesToCheckView::listedPageIds() const {
  std::vector<PageId> pages;
  for (const Page& page : m_model.pages) {
    if (!containsPage(m_dismissedPages, page.pageId)) {
      pages.push_back(page.pageId);
    }
  }
  return pages;
}

void PagesToCheckView::resizeEvent(QResizeEvent* event) {
  QScrollArea::resizeEvent(event);
  const int availableWidth = qMax(kCardWidth, viewport()->width() - 2 * kGridMargin);
  const int columns = qMax(1, (availableWidth + kGridGap) / (kCardWidth + kGridGap));
  if (columns != m_columns) {
    rebuildCards();
  }
}

void PagesToCheckView::rebuildCards() {
  while (QLayoutItem* item = m_grid->takeAt(0)) {
    if (item->widget()) {
      item->widget()->deleteLater();
    }
    delete item;
  }

  const int availableWidth = viewport() ? qMax(kCardWidth, viewport()->width() - 2 * kGridMargin) : kCardWidth;
  m_columns = qMax(1, (availableWidth + kGridGap) / (kCardWidth + kGridGap));

  int visibleIndex = 0;
  for (const Page& page : m_model.pages) {
    if (containsPage(m_dismissedPages, page.pageId)
        || (m_model.visibleGroup >= 0 && page.group != m_model.visibleGroup)) {
      continue;
    }

    auto* card = new QFrame(m_cardsWidget);
    card->setObjectName(QStringLiteral("reviewPageCard"));
    card->setFixedWidth(kCardWidth);
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(12, 12, 12, 12);
    cardLayout->setSpacing(10);

    auto* headerLayout = new QHBoxLayout;
    headerLayout->setContentsMargins(0, 0, 0, 0);
    auto* pageLabel = new QLabel(tr("Page %1").arg(page.pageNumber), card);
    pageLabel->setObjectName(QStringLiteral("reviewPageNumber"));
    auto* metaLabel = new QLabel(page.metaText, card);
    metaLabel->setObjectName(QStringLiteral("reviewPageMeta"));
    metaLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    headerLayout->addWidget(pageLabel);
    headerLayout->addStretch(1);
    headerLayout->addWidget(metaLabel);
    cardLayout->addLayout(headerLayout);

    auto* thumbnailWell = new QLabel(card);
    thumbnailWell->setObjectName(QStringLiteral("reviewThumbnailWell"));
    thumbnailWell->setAlignment(Qt::AlignCenter);
    thumbnailWell->setMinimumHeight(220);
    QPixmap pixmap;
    if (m_thumbnailCache
        && m_thumbnailCache->loadNow(page.pageId.imageId(), pixmap) == ThumbnailPixmapCache::LOADED) {
      thumbnailWell->setPixmap(pixmap.scaled(QSize(232, 208), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    cardLayout->addWidget(thumbnailWell);

    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(8);
    auto* fineButton = new QPushButton(tr("Fine"), card);
    fineButton->setObjectName(QStringLiteral("reviewFineButton"));
    fineButton->setFlat(true);
    fineButton->setMinimumHeight(28);
    connect(fineButton, &QPushButton::clicked, this, [this, page]() { markFine(page.pageId); });
    auto* fixButton = new QToolButton(card);
    fixButton->setObjectName(QStringLiteral("reviewFixButton"));
    fixButton->setText(tr("Fix"));
    fixButton->setPopupMode(QToolButton::InstantPopup);
    fixButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    fixButton->setArrowType(Qt::DownArrow);
    fixButton->setMinimumHeight(28);
    fixButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* menu = new QMenu(fixButton);
    for (const Action& action : page.actions) {
      QAction* menuAction = menu->addAction(action.text);
      connect(menuAction, &QAction::triggered, this, [this, page, action]() {
        if (action.id == ActionId::JumpToPage) {
          emit jumpToPage(page.pageId);
        } else {
          emit actionRequested(page.pageId, action.id);
          markFine(page.pageId);
        }
      });
    }
    fixButton->setMenu(menu);
    buttonLayout->addWidget(fineButton);
    buttonLayout->addWidget(fixButton, 1);
    cardLayout->addLayout(buttonLayout);

    m_grid->addWidget(card, visibleIndex / m_columns, visibleIndex % m_columns);
    ++visibleIndex;
  }

  m_cardsWidget->adjustSize();
}

void PagesToCheckView::markFine(const PageId& pageId) {
  if (!containsPage(m_dismissedPages, pageId)) {
    m_dismissedPages.push_back(pageId);
  }
  rebuildCards();
  const int remaining = static_cast<int>(listedPageIds().size());
  emit remainingChanged(remaining);
  if (remaining == 0) {
    emit allFine();
  }
}
