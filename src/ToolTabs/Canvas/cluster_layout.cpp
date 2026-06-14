#include "cluster_layout.h"
#include "nodes/cluster_group_node.h"
#include "nodes/step_node.h"
#include <QtMath>

ClusterLayout::ClusterLayout()
{
}

void ClusterLayout::arrangeMap(QList<ClusterGroupNode*> clusters)
{
    if (clusters.isEmpty())
        return;

    // Grid layout for clusters
    qreal startX = -500.0;
    qreal startY = -400.0;
    qreal currentX = startX;
    qreal currentY = startY;

    for (int i = 0; i < clusters.size(); ++i) {
        ClusterGroupNode* cluster = clusters[i];
        
        // Position the cluster
        cluster->setPos(currentX, currentY);
        
        // Arrange nodes inside the cluster
        computeLinearChain(cluster);
        
        // Move to next grid position
        currentX += GRID_SPACING + cluster->boundingRect().width();
        
        // Move to next row after COLUMNS
        if ((i + 1) % COLUMNS == 0) {
            currentX = startX;
            currentY += GRID_SPACING + 300.0; // Approximate cluster height
        }
    }
}

void ClusterLayout::computeLinearChain(ClusterGroupNode* cluster)
{
    if (!cluster)
        return;

    QList<StepNode*> children = cluster->children();
    if (children.isEmpty())
        return;

    // Arrange nodes in a vertical chain
    qreal startY = 50.0; // Start below the cluster header
    qreal currentY = startY;

    for (StepNode* node : children) {
        // Center horizontally within cluster
        qreal clusterWidth = cluster->boundingRect().width();
        qreal nodeX = clusterWidth / 2.0;
        
        node->setPos(nodeX, currentY);
        currentY += CHAIN_SPACING;
    }
    
    // Update cluster bounds after arranging children
    cluster->updateBoundsFromChildren();
}
