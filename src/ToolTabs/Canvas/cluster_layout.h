#ifndef CLUSTER_LAYOUT_H
#define CLUSTER_LAYOUT_H

#include <QList>
#include <QPointF>

class ClusterGroupNode;
class StepNode;

class ClusterLayout
{
public:
    ClusterLayout();

    void arrangeMap(QList<ClusterGroupNode*> clusters);
    void computeLinearChain(ClusterGroupNode* cluster);

private:
    static constexpr qreal GRID_SPACING = 100.0;
    static constexpr qreal CHAIN_SPACING = 120.0;
    static constexpr int COLUMNS = 3;
};

#endif // CLUSTER_LAYOUT_H
