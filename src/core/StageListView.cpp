// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "StageListView.h"

#include <QHeaderView>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QTimer>
#include <cassert>
#include <utility>

#include "BubbleAnimation.h"
#include "ChangedStateItemDelegate.h"
#include "ColorSchemeManager.h"
#include "IconProvider.h"
#include "StageSequence.h"

namespace {
constexpr int StageStatusRole = Qt::UserRole + 1;
}

class StageListView::Model : public QAbstractTableModel {
 public:
  Model(QObject* parent, std::shared_ptr<StageSequence> stages);

  void updateBatchProcessingAnimation(int selectedRow, const QPixmap& animationFrame);

  void disableBatchProcessingAnimation();

  void setStageStatus(int row, StageListView::StageStatus status);

  int columnCount(const QModelIndex& parent) const override;

  int rowCount(const QModelIndex& parent) const override;

  QVariant data(const QModelIndex& index, int role) const override;

 private:
  std::shared_ptr<StageSequence> m_stages;
  QPixmap m_curAnimationFrame;
  int m_curSelectedRow;
  std::vector<StageListView::StageStatus> m_statuses;
};


class StageListView::LeftColDelegate : public ChangedStateItemDelegate<QStyledItemDelegate> {
 public:
  explicit LeftColDelegate(StageListView* view);

  void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

 private:
  using SuperClass = ChangedStateItemDelegate<QStyledItemDelegate>;
};


class StageListView::RightColDelegate : public ChangedStateItemDelegate<QStyledItemDelegate> {
 public:
  explicit RightColDelegate(QObject* parent = nullptr);

  void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

 private:
  using SuperClass = ChangedStateItemDelegate<QStyledItemDelegate>;
};


StageListView::StageListView(QWidget* parent)
    : QTableView(parent),
      m_sizeHint(QTableView::sizeHint()),
      m_model(nullptr),
      m_firstColDelegate(new LeftColDelegate(this)),
      m_secondColDelegate(new RightColDelegate(this)),
      m_curBatchAnimationFrame(0),
      m_timerId(0),
      m_batchProcessingPossible(false),
      m_batchProcessingInProgress(false) {
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setAlternatingRowColors(false);

  // Prevent current item visualization. Not to be confused
  // with selected items.
  m_firstColDelegate->flagsForceDisabled(QStyle::State_HasFocus);
  m_secondColDelegate->flagsForceDisabled(QStyle::State_HasFocus);
  setItemDelegateForColumn(0, m_firstColDelegate);
  setItemDelegateForColumn(1, m_secondColDelegate);

  QHeaderView* hHeader = horizontalHeader();
  hHeader->setSectionResizeMode(QHeaderView::Stretch);
  hHeader->hide();

  QHeaderView* vHeader = verticalHeader();
  vHeader->setSectionResizeMode(QHeaderView::Fixed);
  vHeader->setDefaultSectionSize(30);
  vHeader->setMinimumSectionSize(30);
  vHeader->setSectionsMovable(false);
  vHeader->setDefaultAlignment(Qt::AlignRight | Qt::AlignVCenter);
  vHeader->setFixedWidth(24);

  connect(verticalScrollBar(), SIGNAL(rangeChanged(int, int)), this, SLOT(ensureSelectedRowVisible()),
          Qt::QueuedConnection);
}

StageListView::~StageListView() = default;

void StageListView::setStages(const std::shared_ptr<StageSequence>& stages) {
  if (QAbstractItemModel* m = model()) {
    // Q*View classes don't own their models.
    m->deleteLater();
  }

  m_model = new Model(this, stages);
  setModel(m_model);

  QHeaderView* hHeader = horizontalHeader();
  QHeaderView* vHeader = verticalHeader();
  hHeader->setSectionResizeMode(0, QHeaderView::Stretch);
  hHeader->setSectionResizeMode(1, QHeaderView::Fixed);
  hHeader->resizeSection(1, 30);
  if (vHeader->count() != 0) {
    const int reducedSquareSide = std::max(1, vHeader->sectionSize(0) - 6);
    createBatchAnimationSequence(reducedSquareSide);
  } else {
    // Just to avoid special cases elsewhere.
    createBatchAnimationSequence(1);
  }
  m_curBatchAnimationFrame = 0;

  updateRowSpans();
  // Limit the vertical size to make it just enough to get
  // rid of the scrollbars, but not more.
  int height = verticalHeader()->length();
  height += this->height() - viewport()->height();
  m_sizeHint.setHeight(height);
  QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Maximum);
  sp.setVerticalStretch(1);
  setSizePolicy(sp);
  updateGeometry();
}  // StageListView::setStages

void StageListView::setStageStatus(const int row, const StageStatus status) {
  if (m_model) {
    m_model->setStageStatus(row, status);
  }
}

void StageListView::setBatchProcessingPossible(const bool possible) {
  m_batchProcessingPossible = possible;
}

void StageListView::setBatchProcessingInProgress(const bool inProgress) {
  if (m_batchProcessingInProgress == inProgress) {
    return;
  }
  m_batchProcessingInProgress = inProgress;

  if (inProgress) {
    updateRowSpans();  // Join columns.
    // Some styles (Oxygen) visually separate items in a selected row.
    // We really don't want that, so we pretend the items are not selected.
    // Same goes for hovered items.
    m_firstColDelegate->flagsForceDisabled(QStyle::State_Selected | QStyle::State_MouseOver);
    m_secondColDelegate->flagsForceDisabled(QStyle::State_Selected | QStyle::State_MouseOver);

    initiateBatchAnimationFrameRendering();
    m_timerId = startTimer(180);
  } else {
    updateRowSpans();  // Separate columns.

    m_firstColDelegate->removeChanges(QStyle::State_Selected | QStyle::State_MouseOver);
    m_secondColDelegate->removeChanges(QStyle::State_Selected | QStyle::State_MouseOver);

    if (m_model) {
      m_model->disableBatchProcessingAnimation();
    }
    killTimer(m_timerId);
    m_timerId = 0;
  }
}  // StageListView::setBatchProcessingInProgress

void StageListView::timerEvent(QTimerEvent* event) {
  if (event->timerId() != m_timerId) {
    QTableView::timerEvent(event);
    return;
  }

  initiateBatchAnimationFrameRendering();
}

void StageListView::initiateBatchAnimationFrameRendering() {
  if (!m_model || !m_batchProcessingInProgress) {
    return;
  }

  const int selectedRow = this->selectedRow();
  if (selectedRow == -1) {
    return;
  }

  m_model->updateBatchProcessingAnimation(selectedRow, m_batchAnimationPixmaps[m_curBatchAnimationFrame]);
  if (++m_curBatchAnimationFrame == (int) m_batchAnimationPixmaps.size()) {
    m_curBatchAnimationFrame = 0;
  }
}

void StageListView::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) {
  // Call the base version.
  QTableView::selectionChanged(selected, deselected);
}

void StageListView::ensureSelectedRowVisible() {
  // This loop won't run more than one iteration.
  for (const QModelIndex& idx : selectionModel()->selectedRows(0)) {
    scrollTo(idx, EnsureVisible);
  }
}

void StageListView::createBatchAnimationSequence(const int squareSide) {
  const int numFrames = 8;
  m_batchAnimationPixmaps.resize(numFrames);

  const QColor headColor
      = ColorSchemeManager::instance().getColorParam("StageListHead", palette().color(QPalette::Window).darker(200));
  const QColor tailColor
      = ColorSchemeManager::instance().getColorParam("StageListTail", palette().color(QPalette::Window).darker(130));

  BubbleAnimation animation(numFrames);
  for (int i = 0; i < numFrames; ++i) {
    QPixmap& pixmap = m_batchAnimationPixmaps[i];
    if ((pixmap.width() != squareSide) || (pixmap.height() != squareSide)) {
      pixmap = QPixmap(squareSide, squareSide);
    }
    pixmap.fill(Qt::transparent);
    animation.nextFrame(headColor, tailColor, &pixmap);
  }
}

void StageListView::updateRowSpans() {
  clearSpans();
}

int StageListView::selectedRow() const {
  const QModelIndexList selection(selectionModel()->selectedRows(0));
  if (selection.empty()) {
    return -1;
  }
  return selection.front().row();
}

/*========================= StageListView::Model ======================*/

StageListView::Model::Model(QObject* parent, std::shared_ptr<StageSequence> stages)
    : QAbstractTableModel(parent),
      m_stages(std::move(stages)),
      m_curSelectedRow(0),
      m_statuses(m_stages->count(), StageListView::StageStatus::Untouched) {
  assert(m_stages);
}

void StageListView::Model::updateBatchProcessingAnimation(const int selectedRow, const QPixmap& animationFrame) {
  const int minRow = std::min(selectedRow, m_curSelectedRow);
  const int maxRow = std::max(selectedRow, m_curSelectedRow);
  m_curSelectedRow = selectedRow;
  m_curAnimationFrame = animationFrame;
  emit dataChanged(index(minRow, 1), index(maxRow, 1));
}

void StageListView::Model::disableBatchProcessingAnimation() {
  m_curAnimationFrame = QPixmap();
  emit dataChanged(index(0, 1), index(m_curSelectedRow, 1));
}

void StageListView::Model::setStageStatus(const int row, const StageListView::StageStatus status) {
  if (row < 0 || row >= static_cast<int>(m_statuses.size()) || m_statuses[row] == status) {
    return;
  }
  m_statuses[row] = status;
  emit dataChanged(index(row, 1), index(row, 1), {StageStatusRole});
}

int StageListView::Model::columnCount(const QModelIndex& parent) const {
  return 2;
}

int StageListView::Model::rowCount(const QModelIndex& parent) const {
  return m_stages->count();
}

QVariant StageListView::Model::data(const QModelIndex& index, const int role) const {
  if (role == Qt::DisplayRole) {
    if (index.column() == 0) {
      return m_stages->filterAt(index.row())->getName();
    }
  }
  if (role == Qt::UserRole) {
    if (index.column() == 1) {
      if (index.row() == m_curSelectedRow) {
        return m_curAnimationFrame;
      }
    }
  }
  if (role == StageStatusRole && index.column() == 1) {
    return static_cast<int>(m_statuses[index.row()]);
  }
  return QVariant();
}

/*================= StageListView::LeftColDelegate ===================*/

StageListView::LeftColDelegate::LeftColDelegate(StageListView* view) : SuperClass(view) {}

void StageListView::LeftColDelegate::paint(QPainter* painter,
                                           const QStyleOptionViewItem& option,
                                           const QModelIndex& index) const {
  QStyleOptionViewItem stageOption(option);
  const bool selected = option.state.testFlag(QStyle::State_Selected);
  if (selected) {
    painter->fillRect(option.rect, option.palette.color(QPalette::Base));
    stageOption.state &= ~QStyle::State_Selected;
    stageOption.backgroundBrush = option.palette.color(QPalette::Base);
    stageOption.font.setWeight(QFont::DemiBold);
  }
  SuperClass::paint(painter, stageOption, index);
  if (selected) {
    painter->fillRect(QRect(option.rect.left(), option.rect.top(), 3, option.rect.height()), QColor("#2D4FA3"));
  }
}

/*================= StageListView::RightColDelegate ===================*/

StageListView::RightColDelegate::RightColDelegate(QObject* parent) : SuperClass(parent) {}

void StageListView::RightColDelegate::paint(QPainter* painter,
                                            const QStyleOptionViewItem& option,
                                            const QModelIndex& index) const {
  QStyleOptionViewItem stageOption(option);
  if (option.state.testFlag(QStyle::State_Selected)) {
    painter->fillRect(option.rect, option.palette.color(QPalette::Base));
    stageOption.state &= ~QStyle::State_Selected;
    stageOption.backgroundBrush = option.palette.color(QPalette::Base);
  }
  SuperClass::paint(painter, stageOption, index);

  const QVariant var(index.data(Qt::UserRole));
  if (!var.isNull()) {
    const QPixmap pixmap(var.value<QPixmap>());
    if (!pixmap.isNull()) {
      QRect r(pixmap.rect());
      r.moveCenter(option.rect.center());
      painter->drawPixmap(r, pixmap);
      return;
    }
  }

  const auto status = static_cast<StageListView::StageStatus>(index.data(StageStatusRole).toInt());
  const QRect glyphRect(QPoint(0, 0), QSize(16, 16));
  QRect positionedGlyph(glyphRect);
  positionedGlyph.moveCenter(option.rect.center());
  if (status == StageListView::StageStatus::Done) {
    IconProvider::getInstance().getIcon("check-mark").paint(
        painter, positionedGlyph, Qt::AlignCenter, QIcon::Normal, QIcon::On);
  } else if (status == StageListView::StageStatus::Partial) {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const QRectF circle = QRectF(positionedGlyph).adjusted(1.0, 1.0, -1.0, -1.0);
    const QColor color(option.palette.color(QPalette::Text));
    QPainterPath clipPath;
    clipPath.addEllipse(circle);
    painter->setClipPath(clipPath);
    painter->fillRect(QRectF(circle.left(), circle.top(), circle.width() / 2.0, circle.height()), color);
    painter->setClipping(false);
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(color, 1.5));
    painter->drawEllipse(circle);
    painter->restore();
  }
}
