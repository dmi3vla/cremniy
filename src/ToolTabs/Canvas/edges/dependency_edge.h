#ifndef DEPENDENCY_EDGE_H
#define DEPENDENCY_EDGE_H

#include <QGraphicsObject>
#include <QColor>
#include <QVariantAnimation>
#include <QParallelAnimationGroup>

class FileNode;

class DependencyEdge : public QGraphicsObject
{
    Q_OBJECT
    Q_PROPERTY(qreal edgeOpacity READ edgeOpacity WRITE setEdgeOpacity)
    Q_PROPERTY(qreal drawProgress READ drawProgress WRITE setDrawProgress)

public:
    enum EdgeType { Include, Call, Inherit };

    DependencyEdge(FileNode* source, FileNode* target, EdgeType type = Include, QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    void updatePosition();
    FileNode* source() const { return m_source; }
    FileNode* target() const { return m_target; }
    EdgeType edgeType() const { return m_type; }

    void startAnimation();
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
    FileNode* m_source;
    FileNode* m_target;
    EdgeType m_type;
    qreal m_dashOffset = 0;
    qreal m_edgeOpacity = 1.0;
    qreal m_drawProgress = 1.0;
    QVariantAnimation* m_dashAnim = nullptr;
    QParallelAnimationGroup* m_appearGroup = nullptr;
    QParallelAnimationGroup* m_disappearGroup = nullptr;

    QColor edgeColor() const;
    QPainterPath curvePath() const;
};

#endif // DEPENDENCY_EDGE_H
