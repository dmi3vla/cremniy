#ifndef CLUSTER_GROUP_NODE_H
#define CLUSTER_GROUP_NODE_H

#include <QGraphicsItem>
#include <QGraphicsObject>
#include <QPainter>
#include <QString>
#include <QList>

class StepNode;

class ClusterGroupNode : public QGraphicsObject
{
    Q_OBJECT

public:
    explicit ClusterGroupNode(const QString& clusterId, const QString& title, const QString& color, QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    QString clusterId() const { return m_clusterId; }
    QString title() const { return m_title; }
    QString color() const { return m_color; }

    void addChild(StepNode* child);
    void removeChild(StepNode* child);
    QList<StepNode*> children() const { return m_children; }

    void updateBoundsFromChildren();

private:
    QString m_clusterId;
    QString m_title;
    QString m_color;
    QList<StepNode*> m_children;
    QRectF m_bounds;
    static constexpr qreal PADDING = 20;
    static constexpr qreal HEADER_HEIGHT = 30;
    static constexpr qreal CORNER_RADIUS = 12;
};

#endif // CLUSTER_GROUP_NODE_H
