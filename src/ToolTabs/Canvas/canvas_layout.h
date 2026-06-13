#ifndef CANVAS_LAYOUT_H
#define CANVAS_LAYOUT_H

#include <QPointF>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QObject>
#include <QHash>
#include <QParallelAnimationGroup>
#include "dependency_parser.h"

struct LayoutNode {
    QPointF target;
    QString dirPath;
    QStringList files;
    QList<LayoutNode*> children;
};

class CanvasLayout : public QObject
{
    Q_OBJECT

public:
    explicit CanvasLayout(QObject* parent = nullptr);

    void computeTargets(const DependencyGraph& graph, QMap<QString, QPointF>& targets);
    void animateNodesToPositions(
        const QMap<QString, QPointF>& targetPositions,
        QMap<QString, class FileNode*>& nodes,
        int durationMs = 400);

    bool isAnimating() const { return m_posGroup && m_posGroup->state() == QAbstractAnimation::Running; }

signals:
    void animationFinished();

private:
    QParallelAnimationGroup* m_posGroup = nullptr;

    void buildTree(const QStringList& files, LayoutNode& root);
    void layoutTree(LayoutNode& node, qreal startAngle, qreal endAngle, qreal radius, int depth, QMap<QString, QPointF>& targets);
    void cleanupTree(LayoutNode& node);
    QString commonPrefix(const QStringList& paths) const;
    QString relativePath(const QString& path, const QString& base) const;
};

#endif // CANVAS_LAYOUT_H
