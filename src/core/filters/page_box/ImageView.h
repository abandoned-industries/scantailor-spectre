// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_PAGE_BOX_IMAGEVIEW_H_
#define SCANTAILOR_PAGE_BOX_IMAGEVIEW_H_

#include <interaction/DraggablePolygon.h>

#include <QRectF>
#include <QSizeF>

#include "DragHandler.h"
#include "DraggableLineSegment.h"
#include "DraggablePoint.h"
#include "ImageViewBase.h"
#include "ObjectDragHandler.h"
#include "ZoomHandler.h"

class ImageTransformation;

namespace page_box {
class ImageView : public ImageViewBase, private InteractionHandler {
  Q_OBJECT
 public:
  ImageView(const QImage& image,
            const QImage& downscaledImage,
            const ImageTransformation& xform,
            const QRectF& pageRect);

  ~ImageView() override;

 signals:

  void manualPageRectSet(const QRectF& pageRect);

  void pageRectSizeChanged(const QSizeF& size);

 public slots:

  void pageRectSetExternally(const QRectF& pageRect);

 private:
  enum Edge { LEFT = 1, RIGHT = 2, TOP = 4, BOTTOM = 8 };

  void onPaint(QPainter& painter, const InteractionState& interaction) override;

  QPointF pageRectCornerPosition(int edgeMask) const;

  void pageRectCornerMoveRequest(int edgeMask, const QPointF& pos);

  QLineF pageRectEdgePosition(int edge) const;

  void pageRectEdgeMoveRequest(int edge, const QLineF& line);

  void pageRectDragFinished();

  void forceInsideImage(QRectF& widgetRect, int edgeMask) const;

  QRectF pageRectPosition() const;

  void pageRectMoveRequest(const QPolygonF& polyMoved);

  void enablePageRectInteraction(bool state);


  DraggablePoint m_pageRectCorners[4];
  ObjectDragHandler m_pageRectCornerHandlers[4];

  DraggableLineSegment m_pageRectEdges[4];
  ObjectDragHandler m_pageRectEdgeHandlers[4];

  DraggablePolygon m_pageRectArea;
  ObjectDragHandler m_pageRectAreaHandler;

  DragHandler m_dragHandler;
  ZoomHandler m_zoomHandler;

  /**
   * Page rect in virtual image coordinates.
   */
  QRectF m_pageRect;

  QSizeF m_minBoxSize;
};
}  // namespace page_box
#endif  // ifndef SCANTAILOR_PAGE_BOX_IMAGEVIEW_H_
