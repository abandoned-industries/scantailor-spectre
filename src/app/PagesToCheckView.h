// Copyright (C) 2026  ScanTailor Spectre contributors.
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_APP_PAGESTOCHECKVIEW_H_
#define SCANTAILOR_APP_PAGESTOCHECKVIEW_H_

#include <QScrollArea>
#include <QString>
#include <memory>
#include <vector>

#include "PageId.h"

class QGridLayout;
class ThumbnailPixmapCache;
class QWidget;

class PagesToCheckView : public QScrollArea {
  Q_OBJECT

 public:
  enum class ActionId {
    JumpToPage,
    ForceTwoPageLayout,
    ForceSinglePageLayout,
    ResetAutoLayout,
    DisablePageBox,
    PreserveLayout,
    DetachFromAggregateSize,
    GoToSplitPages
  };

  struct Action {
    ActionId id;
    QString text;
  };

  struct Page {
    PageId pageId;
    int pageNumber = 0;
    QString metaText;
    std::vector<Action> actions;
    int group = 0;
  };

  struct ReviewModel {
    std::vector<Page> pages;
    int totalPages = 0;
    int visibleGroup = -1;
  };

  PagesToCheckView(const ReviewModel& model,
                   std::shared_ptr<ThumbnailPixmapCache> thumbnailCache,
                   QWidget* parent = nullptr);

  void setModel(const ReviewModel& model);
  std::vector<PageId> listedPageIds() const;

 signals:
  void jumpToPage(const PageId& pageId);
  void actionRequested(const PageId& pageId, PagesToCheckView::ActionId actionId);
  void remainingChanged(int count);
  void allFine();

 protected:
  void resizeEvent(QResizeEvent* event) override;

 private:
  void rebuildCards();
  void markFine(const PageId& pageId);

  ReviewModel m_model;
  std::shared_ptr<ThumbnailPixmapCache> m_thumbnailCache;
  QWidget* m_cardsWidget;
  QGridLayout* m_grid;
  std::vector<PageId> m_dismissedPages;
  int m_columns = 0;
};

#endif  // SCANTAILOR_APP_PAGESTOCHECKVIEW_H_
