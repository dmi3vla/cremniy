#include "minimap.h"
#include "canvas_view.h"
#include <QMouseEvent>
#include <QPainter>

Minimap::Minimap(CanvasView* mainView, QWidget* parent)
    : QGraphicsView(parent)
    , m_mainView(mainView)
{
    setFixedSize(MINIMAP_WIDTH, MINIMAP_HEIGHT);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setRenderHint(QPainter::Antialiasing);
    setBackgroundBrush(QColor(30, 30, 35));
    setInteractive(false);

    if (m_mainView && m_mainView->scene()) {
        setScene(m_mainView->scene());
        fitInView(sceneRect(), Qt::KeepAspectRatio);
    }
}

void Minimap::updateViewportRect()
{
    if (!m_mainView) return;

    QRectF viewportRect = m_mainView->mapToScene(m_mainView->viewport()->rect()).boundingRect();

    if (m_viewportRect) {
        m_viewportRect->setRect(viewportRect);
    } else {
        m_viewportRect = scene()->addRect(viewportRect, QPen(QColor(220, 60, 60, 180), 2));
        m_viewportRect->setZValue(1000);
    }

    fitInView(sceneRect(), Qt::KeepAspectRatio);
}

void Minimap::drawForeground(QPainter* painter, const QRectF& rect)
{
    Q_UNUSED(rect)
    if (!m_mainView) return;

    QRectF viewportRect = m_mainView->mapToScene(m_mainView->viewport()->rect()).boundingRect();
    QRectF mapped = mapFromScene(viewportRect).boundingRect();

    painter->setPen(QPen(QColor(220, 60, 60, 180), 2));
    painter->setBrush(QColor(220, 60, 60, 30));
    painter->drawRect(mapped);
}

void Minimap::mousePressEvent(QMouseEvent* event)
{
    QPointF scenePos = mapToScene(event->pos());
    navigateTo(scenePos);
}

void Minimap::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton) {
        QPointF scenePos = mapToScene(event->pos());
        navigateTo(scenePos);
    }
}

void Minimap::navigateTo(QPointF scenePos)
{
    if (m_mainView)
        m_mainView->centerOn(scenePos);
}
