// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "ImageView.h"

#include <QPainter>
#include <boost/bind/bind.hpp>
#include <cmath>

#include "ImagePresentation.h"
#include "ImageTransformation.h"

namespace page_box {
ImageView::ImageView(const QImage& image,
                     const QImage& downscaledImage,
                     const ImageTransformation& xform,
                     const QRectF& pageRect)
    : ImageViewBase(image, downscaledImage, ImagePresentation(xform.transform(), xform.resultingPreCropArea())),
      m_dragHandler(*this),
      m_zoomHandler(*this),
      m_pageRect(pageRect),
      m_minBoxSize(10.0, 10.0) {
  setMouseTracking(true);

  interactionState().setDefaultStatusTip(
      tr("Drag lines or corners to resize the page box. Hold Shift to drag the whole box."));

  const QString pageRectDragTip(tr("Drag lines or corners to resize the page box."));
  static const int masks_by_edge[] = {TOP, RIGHT, BOTTOM, LEFT};
  static const int masks_by_corner[] = {TOP | LEFT, TOP | RIGHT, BOTTOM | RIGHT, BOTTOM | LEFT};
  for (int i = 0; i < 4; ++i) {
    // Proximity priority - corners higher than edges.
    m_pageRectCorners[i].setProximityPriority(2);
    m_pageRectEdges[i].setProximityPriority(1);

    // Setup corner drag handlers.
    m_pageRectCorners[i].setPositionCallback(
        boost::bind(&ImageView::pageRectCornerPosition, this, masks_by_corner[i]));
    m_pageRectCorners[i].setMoveRequestCallback(
        boost::bind(&ImageView::pageRectCornerMoveRequest, this, masks_by_corner[i], boost::placeholders::_1));
    m_pageRectCorners[i].setDragFinishedCallback(boost::bind(&ImageView::pageRectDragFinished, this));
    m_pageRectCornerHandlers[i].setObject(&m_pageRectCorners[i]);
    m_pageRectCornerHandlers[i].setProximityStatusTip(pageRectDragTip);

    // Setup edge drag handlers.
    m_pageRectEdges[i].setPositionCallback(boost::bind(&ImageView::pageRectEdgePosition, this, masks_by_edge[i]));
    m_pageRectEdges[i].setMoveRequestCallback(
        boost::bind(&ImageView::pageRectEdgeMoveRequest, this, masks_by_edge[i], boost::placeholders::_1));
    m_pageRectEdges[i].setDragFinishedCallback(boost::bind(&ImageView::pageRectDragFinished, this));
    m_pageRectEdgeHandlers[i].setObject(&m_pageRectEdges[i]);
    m_pageRectEdgeHandlers[i].setProximityStatusTip(pageRectDragTip);

    Qt::CursorShape cornerCursor = (i & 1) ? Qt::SizeBDiagCursor : Qt::SizeFDiagCursor;
    m_pageRectCornerHandlers[i].setProximityCursor(cornerCursor);
    m_pageRectCornerHandlers[i].setInteractionCursor(cornerCursor);

    Qt::CursorShape edgeCursor = (i & 1) ? Qt::SizeHorCursor : Qt::SizeVerCursor;
    m_pageRectEdgeHandlers[i].setProximityCursor(edgeCursor);
    m_pageRectEdgeHandlers[i].setInteractionCursor(edgeCursor);

    makeLastFollower(m_pageRectCornerHandlers[i]);
    makeLastFollower(m_pageRectEdgeHandlers[i]);
  }

  {
    // Setup rectangle drag interaction (whole-box drag with Shift held).
    m_pageRectArea.setProximityPriority(0);

    m_pageRectArea.setPositionCallback(boost::bind(&ImageView::pageRectPosition, this));
    m_pageRectArea.setMoveRequestCallback(boost::bind(&ImageView::pageRectMoveRequest, this, boost::placeholders::_1));
    m_pageRectArea.setDragFinishedCallback(boost::bind(&ImageView::pageRectDragFinished, this));
    m_pageRectAreaHandler.setObject(&m_pageRectArea);
    m_pageRectAreaHandler.setProximityStatusTip(tr("Hold left mouse button to drag the page box."));
    m_pageRectAreaHandler.setInteractionStatusTip(tr("Release left mouse button to finish dragging."));

    Qt::CursorShape cursor = Qt::DragMoveCursor;
    m_pageRectAreaHandler.setKeyboardModifiers({Qt::ShiftModifier});
    m_pageRectAreaHandler.setProximityCursor(cursor);
    m_pageRectAreaHandler.setInteractionCursor(cursor);

    makeLastFollower(m_pageRectAreaHandler);
  }

  rootInteractionHandler().makeLastFollower(*this);
  rootInteractionHandler().makeLastFollower(m_dragHandler);
  rootInteractionHandler().makeLastFollower(m_zoomHandler);
}

ImageView::~ImageView() = default;

void ImageView::onPaint(QPainter& painter, const InteractionState& interaction) {
  if (m_pageRect.isNull()) {
    return;
  }

  painter.setRenderHints(QPainter::Antialiasing, true);

  // Draw the page bounding box in orange.
  QPen pen(QColor(0xff, 0x7f, 0x00));
  pen.setWidthF(2.0);
  pen.setCosmetic(true);
  painter.setPen(pen);

  painter.setBrush(Qt::NoBrush);

  painter.drawRect(m_pageRect);
}

QPointF ImageView::pageRectCornerPosition(int edgeMask) const {
  const QRectF rect(virtualToWidget().mapRect(m_pageRect));
  QPointF pt;

  if (edgeMask & TOP) {
    pt.setY(rect.top());
  } else if (edgeMask & BOTTOM) {
    pt.setY(rect.bottom());
  }

  if (edgeMask & LEFT) {
    pt.setX(rect.left());
  } else if (edgeMask & RIGHT) {
    pt.setX(rect.right());
  }
  return pt;
}

void ImageView::pageRectCornerMoveRequest(int edgeMask, const QPointF& pos) {
  QRectF r(virtualToWidget().mapRect(m_pageRect));
  const qreal minw = m_minBoxSize.width();
  const qreal minh = m_minBoxSize.height();

  if (edgeMask & TOP) {
    r.setTop(std::min(pos.y(), r.bottom() - minh));
  } else if (edgeMask & BOTTOM) {
    r.setBottom(std::max(pos.y(), r.top() + minh));
  }

  if (edgeMask & LEFT) {
    r.setLeft(std::min(pos.x(), r.right() - minw));
  } else if (edgeMask & RIGHT) {
    r.setRight(std::max(pos.x(), r.left() + minw));
  }

  forceInsideImage(r, edgeMask);
  m_pageRect = widgetToVirtual().mapRect(r);

  update();
  emit pageRectSizeChanged(m_pageRect.size());
}

QLineF ImageView::pageRectEdgePosition(int edge) const {
  const QRectF rect(virtualToWidget().mapRect(m_pageRect));

  if (edge == TOP) {
    return QLineF(rect.topLeft(), rect.topRight());
  } else if (edge == BOTTOM) {
    return QLineF(rect.bottomLeft(), rect.bottomRight());
  } else if (edge == LEFT) {
    return QLineF(rect.topLeft(), rect.bottomLeft());
  } else {
    return QLineF(rect.topRight(), rect.bottomRight());
  }
}

void ImageView::pageRectEdgeMoveRequest(int edge, const QLineF& line) {
  pageRectCornerMoveRequest(edge, line.p1());
}

void ImageView::pageRectDragFinished() {
  emit manualPageRectSet(m_pageRect);
}

void ImageView::forceInsideImage(QRectF& widgetRect, const int edgeMask) const {
  const qreal minw = m_minBoxSize.width();
  const qreal minh = m_minBoxSize.height();
  const QRectF imageRect(virtualToWidget().mapRect(virtualDisplayRect()));

  if ((edgeMask & LEFT) && (widgetRect.left() < imageRect.left())) {
    widgetRect.setLeft(imageRect.left());
    widgetRect.setRight(std::max(widgetRect.right(), widgetRect.left() + minw));
  }
  if ((edgeMask & RIGHT) && (widgetRect.right() > imageRect.right())) {
    widgetRect.setRight(imageRect.right());
    widgetRect.setLeft(std::min(widgetRect.left(), widgetRect.right() - minw));
  }
  if ((edgeMask & TOP) && (widgetRect.top() < imageRect.top())) {
    widgetRect.setTop(imageRect.top());
    widgetRect.setBottom(std::max(widgetRect.bottom(), widgetRect.top() + minh));
  }
  if ((edgeMask & BOTTOM) && (widgetRect.bottom() > imageRect.bottom())) {
    widgetRect.setBottom(imageRect.bottom());
    widgetRect.setTop(std::min(widgetRect.top(), widgetRect.bottom() - minh));
  }
}

QRectF ImageView::pageRectPosition() const {
  return virtualToWidget().mapRect(m_pageRect);
}

void ImageView::pageRectMoveRequest(const QPolygonF& polyMoved) {
  QRectF pageRectInWidget(polyMoved.boundingRect());

  // Clamp the page rect to stay inside the image bounds.
  const QRectF imageRect(virtualToWidget().mapRect(virtualDisplayRect()));
  if (pageRectInWidget.left() < imageRect.left()) {
    pageRectInWidget.translate(imageRect.left() - pageRectInWidget.left(), 0);
  }
  if (pageRectInWidget.right() > imageRect.right()) {
    pageRectInWidget.translate(imageRect.right() - pageRectInWidget.right(), 0);
  }
  if (pageRectInWidget.top() < imageRect.top()) {
    pageRectInWidget.translate(0, imageRect.top() - pageRectInWidget.top());
  }
  if (pageRectInWidget.bottom() > imageRect.bottom()) {
    pageRectInWidget.translate(0, imageRect.bottom() - pageRectInWidget.bottom());
  }

  m_pageRect = widgetToVirtual().mapRect(pageRectInWidget);

  update();
  emit pageRectSizeChanged(m_pageRect.size());
}

void ImageView::pageRectSetExternally(const QRectF& pageRect) {
  m_pageRect = pageRect;
  update();
  emit pageRectSizeChanged(m_pageRect.size());
}

void ImageView::enablePageRectInteraction(const bool state) {
  if (state) {
    for (int i = 0; i < 4; ++i) {
      makeLastFollower(m_pageRectCornerHandlers[i]);
      makeLastFollower(m_pageRectEdgeHandlers[i]);
    }
    makeLastFollower(m_pageRectAreaHandler);
  } else {
    for (int i = 0; i < 4; ++i) {
      m_pageRectCornerHandlers[i].unlink();
      m_pageRectEdgeHandlers[i].unlink();
    }
    m_pageRectAreaHandler.unlink();
  }
}
}  // namespace page_box
