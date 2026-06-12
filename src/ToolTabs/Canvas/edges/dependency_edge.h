#ifndef DEPENDENCY_EDGE_H
#define DEPENDENCY_EDGE_H

#include <QGraphicsObject>
#include <QColor>
#include <QVariantAnimation>

class FileNode;

class DependencyEdge : public QGraphicsObject
{
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

private:
    FileNode* m_source;
    FileNode* m_target;
    EdgeType m_type;
    qreal m_dashOffset = 0;
    QVariantAnimation* m_dashAnim = nullptr;

    QColor edgeColor() const;
    QPainterPath curvePath() const;
};

#endif // DEPENDENCY_EDGE_H
