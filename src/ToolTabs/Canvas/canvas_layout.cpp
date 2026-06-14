#include "canvas_layout.h"
#include "nodes/file_node.h"
#include <QDir>
#include <QFileInfo>
#include <QtMath>

CanvasLayout::CanvasLayout(QObject* parent)
    : QObject(parent)
{
}

void CanvasLayout::computeTargets(const DependencyGraph& graph, QMap<QString, QPointF>& targets)
{
    targets.clear();
    if (graph.allFiles.isEmpty())
        return;

    LayoutNode root;
    buildTree(graph.allFiles, root);
    layoutTree(root, 0, 2 * M_PI, 200.0, 0, targets);
    cleanupTree(root);
}

QMap<QString, QPointF> CanvasLayout::computeLinearChain(
    const QStringList& orderedKeys,
    qreal spacing,
    qreal y,
    Qt::Orientation orientation)
{
    QMap<QString, QPointF> positions;
    if (orderedKeys.isEmpty())
        return positions;

    qreal totalLen = (orderedKeys.size() - 1) * spacing;
    qreal startOffset = -totalLen / 2.0;

    for (int i = 0; i < orderedKeys.size(); ++i) {
        qreal pos = startOffset + i * spacing;
        if (orientation == Qt::Horizontal)
            positions[orderedKeys[i]] = QPointF(pos, y);
        else
            positions[orderedKeys[i]] = QPointF(y, pos);
    }

    return positions;
}

void CanvasLayout::buildTree(const QStringList& files, LayoutNode& root)
{
    root.dirPath = commonPrefix(files);
    if (root.dirPath.isEmpty())
        return;

    QHash<QString, QStringList> dirFiles;
    QHash<QString, QStringList> subDirs;

    for (const QString& file : files) {
        QFileInfo fi(file);
        QString dir = fi.absolutePath();

        if (dir == root.dirPath) {
            root.files.append(file);
        } else {
            QString rel = relativePath(dir, root.dirPath);
            QStringList parts = rel.split('/', Qt::SkipEmptyParts);

            if (!parts.isEmpty()) {
                QString firstDir = parts.first();
                if (!dirFiles.contains(firstDir)) {
                    subDirs[root.dirPath].append(firstDir);
                }
                dirFiles[firstDir].append(file);
            }
        }
    }

    const QStringList& childDirs = subDirs[root.dirPath];
    for (const QString& dirName : childDirs) {
        LayoutNode* child = new LayoutNode;
        child->dirPath = QDir(root.dirPath).filePath(dirName);

        for (const QString& file : dirFiles[dirName]) {
            QFileInfo fi(file);
            if (fi.absolutePath() == child->dirPath) {
                child->files.append(file);
            } else {
                QString rel = relativePath(fi.absolutePath(), child->dirPath);
                QStringList parts = rel.split('/', Qt::SkipEmptyParts);
                if (!parts.isEmpty()) {
                    child->files.append(file);
                }
            }
        }

        buildTree(child->files, *child);
        root.children.append(child);
    }
}

void CanvasLayout::layoutTree(LayoutNode& node, qreal startAngle, qreal endAngle,
                               qreal radius, int depth, QMap<QString, QPointF>& targets)
{
    qreal cx = 0, cy = 0;
    if (depth > 0) {
        qreal midAngle = (startAngle + endAngle) / 2;
        cx = radius * qCos(midAngle);
        cy = radius * qSin(midAngle);
    }

    int fileCount = node.files.size();
    if (fileCount > 0) {
        qreal fileRadius = radius + 40;
        qreal fileSpread = (endAngle - startAngle) * 0.4;
        qreal fileAngleStart = (startAngle + endAngle) / 2 - fileSpread / 2;

        for (int i = 0; i < fileCount; ++i) {
            qreal angle = fileAngleStart + (fileCount > 1 ? fileSpread * i / (fileCount - 1) : 0);
            qreal fx = cx + fileRadius * qCos(angle);
            qreal fy = cy + fileRadius * qSin(angle);
            targets[node.files[i]] = QPointF(fx, fy);
        }
    }

    int childCount = node.children.size();
    if (childCount == 0)
        return;

    qreal childRadius = radius + 120;
    qreal angleRange = endAngle - startAngle;
    qreal anglePerChild = angleRange / childCount;

    for (int i = 0; i < childCount; ++i) {
        qreal childStart = startAngle + i * anglePerChild;
        qreal childEnd = childStart + anglePerChild;
        layoutTree(*node.children[i], childStart, childEnd, childRadius, depth + 1, targets);
    }
}

void CanvasLayout::animateNodesToPositions(
    const QMap<QString, QPointF>& targetPositions,
    QMap<QString, QGraphicsObject*>& nodes,
    int durationMs)
{
    if (m_posGroup) {
        m_posGroup->stop();
        m_posGroup->deleteLater();
    }

    m_posGroup = new QParallelAnimationGroup(this);

    for (auto it = targetPositions.begin(); it != targetPositions.end(); ++it) {
        if (!nodes.contains(it.key())) continue;
        QGraphicsObject* node = nodes[it.key()];

        auto* anim = new QPropertyAnimation(node, "pos");
        anim->setDuration(durationMs);
        anim->setStartValue(node->pos());
        anim->setEndValue(it.value());
        anim->setEasingCurve(QEasingCurve::InOutCubic);
        m_posGroup->addAnimation(anim);
    }

    connect(m_posGroup, &QParallelAnimationGroup::finished, this, &CanvasLayout::animationFinished);
    m_posGroup->start();
}

void CanvasLayout::cleanupTree(LayoutNode& node)
{
    for (auto* child : node.children)
        cleanupTree(*child);
    qDeleteAll(node.children);
    node.children.clear();
}

QString CanvasLayout::commonPrefix(const QStringList& paths) const
{
    if (paths.isEmpty())
        return QString();

    QString prefix = paths.first();
    for (int i = 1; i < paths.size(); ++i) {
        while (!paths[i].startsWith(prefix)) {
            prefix.chop(1);
            int lastSep = prefix.lastIndexOf('/');
            if (lastSep < 0) return QString();
            prefix = prefix.left(lastSep);
        }
    }

    QFileInfo fi(prefix);
    if (fi.isFile())
        return fi.absolutePath();
    return prefix;
}

QString CanvasLayout::relativePath(const QString& path, const QString& base) const
{
    if (path.startsWith(base)) {
        QString rel = path.mid(base.length());
        if (rel.startsWith('/'))
            rel = rel.mid(1);
        return rel;
    }
    return path;
}
