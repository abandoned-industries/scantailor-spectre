// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_CORE_STAGELISTVIEW_H_
#define SCANTAILOR_CORE_STAGELISTVIEW_H_

#include <QPixmap>
#include <QTableView>
#include <memory>
#include <vector>

class StageSequence;

class StageListView : public QTableView {
  Q_OBJECT
 public:
  enum class StageStatus { Untouched, Partial, Done };

  explicit StageListView(QWidget* parent);

  ~StageListView() override;

  void setStages(const std::shared_ptr<StageSequence>& stages);

  void setStageStatus(int row, StageStatus status);

  QSize sizeHint() const override { return m_sizeHint; }

 public slots:

  void setBatchProcessingPossible(bool possible);

  void setBatchProcessingInProgress(bool inProgress);

 protected slots:

  void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) override;

 private slots:

  void ensureSelectedRowVisible();

 protected:
  void timerEvent(QTimerEvent* event) override;

 private:
  class Model;
  class LeftColDelegate;

  class RightColDelegate;

  void initiateBatchAnimationFrameRendering();

  void createBatchAnimationSequence(int squareSide);

  void updateRowSpans();

  int selectedRow() const;

  QSize m_sizeHint;
  Model* m_model;
  LeftColDelegate* m_firstColDelegate;
  RightColDelegate* m_secondColDelegate;
  std::vector<QPixmap> m_batchAnimationPixmaps;
  int m_curBatchAnimationFrame;
  int m_timerId;
  bool m_batchProcessingPossible;
  bool m_batchProcessingInProgress;
};


#endif  // ifndef SCANTAILOR_CORE_STAGELISTVIEW_H_
