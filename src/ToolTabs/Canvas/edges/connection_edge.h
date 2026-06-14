#ifndef CONNECTION_EDGE_H
#define CONNECTION_EDGE_H

#include <QGraphicsObject>
#include <QColor>
#include <QVariantAnimation>
#include <QParallelAnimationGroup>

class StepNode;

class ConnectionEdge : public QGraphicsObject
{
    Q_OBJECT
    Q_PROPERTY(qreal edgeOpacity READ edgeOpacity WRITE setEdgeOpacity)
    Q_PROPERTY(qreal drawProgress READ drawProgress WRITE setDrawProgress)

public:
    ConnectionEdge(StepNode* source, StepNode* target, const QString& relationLabel, QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    void updatePosition();
    StepNode* source() const { return m_source; }
    StepNode* target() const { return m_target; }
    QString relationLabel() const { return m_relationLabel; }

    void playAppearAnimation(int durationMs = 300);
    void playDisappearAnimation(int durationMs = 250);
    void stopAnimations();

    qreal edgeOpacity() const { return m_edgeOpacity; }
    void setEdgeOpacity(qreal opacity) { m_edgeOpacity = opacity; update(); }
    qreal drawProgress() const { return m_drawProgress; }
    void setDrawProgress(qreal progress) { m_drawProgress = progress; update(); }

signals:
    void disappearFinished();

private:
    StepNode* m_source;
    StepNode* m_target;
    QString m_relationLabel;
    qreal m_edgeOpacity = 1.0;
    qreal m_drawProgress = 1.0;
    QParallelAnimationGroup* m_appearGroup = nullptr;
    QParallelAnimationGroup* m_disappearGroup = nullptr;

    QColor edgeColor() const;
    QPainterPath curvePath() const;
};

#endif // CONNECTION_EDGE_H
