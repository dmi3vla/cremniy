#include "canvas_view.h"
#include <QWheelEvent>
#include <QPainter>
#include <QScrollBar>
#include <QtMath>

CanvasView::CanvasView(QWidget* parent)
    : QGraphicsView(parent)
{
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);

    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::NoDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    setViewportUpdateMode(SmartViewportUpdate);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    setBackgroundBrush(QColor(25, 25, 30));
    setSceneRect(-50000, -50000, 100000, 100000);
    centerOn(0, 0);
}

void CanvasView::wheelEvent(QWheelEvent* event)
{
    if (event->angleDelta().y() > 0)
        zoomIn();
    else
        zoomOut();
    event->accept();
}

void CanvasView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton) {
        m_panning = true;
        m_lastPanPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && items(event->pos()).isEmpty()) {
        m_panning = true;
        m_lastPanPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void CanvasView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_panning) {
        QPointF delta = mapToScene(event->pos()) - mapToScene(m_lastPanPos.toPoint());
        m_lastPanPos = event->pos();
        centerOn(sceneRect().center() - delta);
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void CanvasView::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_panning) {
        m_panning = false;
        unsetCursor();
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void CanvasView::zoomIn()
{
    if (m_zoomFactor * ZOOM_STEP <= MAX_ZOOM) {
        m_zoomFactor *= ZOOM_STEP;
        applyZoom();
    }
}

void CanvasView::zoomOut()
{
    if (m_zoomFactor / ZOOM_STEP >= MIN_ZOOM) {
        m_zoomFactor /= ZOOM_STEP;
        applyZoom();
    }
}

void CanvasView::resetZoom()
{
    m_zoomFactor = 1.0;
    applyZoom();
    centerOn(0, 0);
}

void CanvasView::applyZoom()
{
    QTransform transform;
    transform.scale(m_zoomFactor, m_zoomFactor);
    setTransform(transform);
}

void CanvasView::drawBackground(QPainter* painter, const QRectF& rect)
{
    QGraphicsView::drawBackground(painter, rect);
    drawGrid(painter, rect);
}

void CanvasView::drawGrid(QPainter* painter, const QRectF& rect)
{
    const int gridSize = 50;

    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(60, 60, 70));

    qreal left = qFloor(rect.left() / gridSize) * gridSize;
    qreal top = qFloor(rect.top() / gridSize) * gridSize;

    for (qreal x = left; x <= rect.right(); x += gridSize) {
        for (qreal y = top; y <= rect.bottom(); y += gridSize) {
            painter->drawEllipse(QPointF(x, y), 1.5, 1.5);
        }
    }
}
