#ifndef CANVAS_VIEW_H
#define CANVAS_VIEW_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QPointF>

class CanvasView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit CanvasView(QWidget* parent = nullptr);

    void zoomIn();
    void zoomOut();
    void resetZoom();
    qreal zoomFactor() const { return m_zoomFactor; }

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void drawBackground(QPainter* painter, const QRectF& rect) override;

private:
    QGraphicsScene* m_scene;
    bool m_panning = false;
    QPointF m_lastPanPos;
    qreal m_zoomFactor = 1.0;
    static constexpr qreal MIN_ZOOM = 0.1;
    static constexpr qreal MAX_ZOOM = 10.0;
    static constexpr qreal ZOOM_STEP = 1.15;

    void applyZoom();
    void drawGrid(QPainter* painter, const QRectF& rect);
};

#endif // CANVAS_VIEW_H
