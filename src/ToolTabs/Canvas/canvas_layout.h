#ifndef CANVAS_LAYOUT_H
#define CANVAS_LAYOUT_H

#include <QPointF>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QObject>
#include <QHash>
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
    void animateToTargets(QMap<QString, class FileNode*>& nodes, int durationMs = 600);

    bool isAnimating() const { return m_animationTimer.isActive(); }

signals:
    void animationFinished();

private:
    QTimer m_animationTimer;
    QMap<QString, QPointF> m_startPos;
    QMap<QString, QPointF> m_targetPos;
    QMap<QString, class FileNode*>* m_animNodes = nullptr;
    int m_animationStep = 0;
    int m_animationSteps = 0;

    void buildTree(const QStringList& files, LayoutNode& root);
    void layoutTree(LayoutNode& node, qreal startAngle, qreal endAngle, qreal radius, int depth, QMap<QString, QPointF>& targets);
    void cleanupTree(LayoutNode& node);
    void animationTick();
    QString commonPrefix(const QStringList& paths) const;
    QString relativePath(const QString& path, const QString& base) const;
};

#endif // CANVAS_LAYOUT_H
