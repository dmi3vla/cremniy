#ifndef MINIMAP_H
#define MINIMAP_H

#include <QGraphicsView>
#include <QGraphicsRectItem>

class CanvasView;

class Minimap : public QGraphicsView
{
    Q_OBJECT

public:
    explicit Minimap(CanvasView* mainView, QWidget* parent = nullptr);

    void updateViewportRect();
    void setMainView(CanvasView* view) { m_mainView = view; }

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void drawForeground(QPainter* painter, const QRectF& rect) override;

private:
    CanvasView* m_mainView;
    QGraphicsRectItem* m_viewportRect = nullptr;
    static constexpr int MINIMAP_WIDTH = 160;
    static constexpr int MINIMAP_HEIGHT = 120;

    void navigateTo(QPointF scenePos);
};

#endif // MINIMAP_H
