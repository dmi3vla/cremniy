#include "dependency_edge.h"
#include "../nodes/file_node.h"
#include <QPainter>
#include <QPen>
#include <QLineF>
#include <QtMath>

DependencyEdge::DependencyEdge(FileNode* source, FileNode* target, EdgeType type, QGraphicsItem* parent)
    : QGraphicsObject(parent)
    , m_source(source)
    , m_target(target)
    , m_type(type)
{
    setZValue(-1);

    if (m_source)
        connect(m_source, &FileNode::positionChanged, this, &DependencyEdge::updatePosition);
    if (m_target)
        connect(m_target, &FileNode::positionChanged, this, &DependencyEdge::updatePosition);

    startAnimation();
}

QRectF DependencyEdge::boundingRect() const
{
    if (!m_source || !m_target)
        return QRectF();

    QPainterPath path = curvePath();
    return path.boundingRect().adjusted(-15, -15, 15, 15);
}

void DependencyEdge::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    if (!m_source || !m_target)
        return;

    QPainterPath path = curvePath();
    if (path.isEmpty())
        return;

    painter->setRenderHint(QPainter::Antialiasing);

    // Apply edge opacity
    QColor color = edgeColor();
    color.setAlphaF(m_edgeOpacity * (color.alphaF()));

    // Draw partial path based on drawProgress
    QPainterPath drawPath;
    if (m_drawProgress < 0.999) {
        QPointF p1 = m_source->pos();
        QPointF p2 = m_target->pos();
        QPointF cp1(p1.x() + (p2.x() - p1.x()) * 0.33,
                     p1.y() + qMin((p2.x() - p1.x()) * 0.3, 80.0));
        QPointF cp2(p1.x() + (p2.x() - p1.x()) * 0.67,
                     p2.y() - qMin((p2.x() - p1.x()) * 0.3, 80.0));

        drawPath.moveTo(p1);
        // Interpolate cubic bezier by drawProgress
        qreal t = m_drawProgress;
        qreal t2 = t * t;
        qreal t3 = t2 * t;
        qreal mt = 1.0 - t;
        qreal mt2 = mt * mt;
        qreal mt3 = mt2 * mt;

        QPointF end = mt3 * p1 + 3 * mt2 * t * cp1 + 3 * mt * t2 * cp2 + t3 * p2;
        QPointF cp1Interp = mt2 * p1 + 2 * mt * t * cp1 + t2 * cp2;
        drawPath.cubicTo(cp1Interp, end, end);
    } else {
        drawPath = path;
    }

    QPen pen(color, 1.5, Qt::DashLine);
    pen.setDashOffset(m_dashOffset);
    painter->setPen(pen);
    painter->drawPath(drawPath);

    // Arrow at the end (only when mostly drawn)
    if (m_drawProgress >= 0.95) {
        QPointF end = path.pointAtPercent(1.0);
        QPointF before = path.pointAtPercent(0.95);
        double angle = std::atan2(end.y() - before.y(), end.x() - before.x());

        QPointF arrowP1 = end - QPointF(qSin(angle + M_PI / 3) * 10, qCos(angle + M_PI / 3) * 10);
        QPointF arrowP2 = end - QPointF(qSin(angle + M_PI - M_PI / 3) * 10, qCos(angle + M_PI - M_PI / 3) * 10);

        QPolygonF arrowHead;
        arrowHead << end << arrowP1 << arrowP2;
        painter->setBrush(color);
        painter->setPen(Qt::NoPen);
        painter->drawPolygon(arrowHead);
    }
}

void DependencyEdge::updatePosition()
{
    prepareGeometryChange();
}

void DependencyEdge::startAnimation()
{
    m_dashAnim = new QVariantAnimation(this);
    m_dashAnim->setDuration(2000);
    m_dashAnim->setStartValue(0.0);
    m_dashAnim->setEndValue(-40.0);
    m_dashAnim->setLoopCount(-1);
    connect(m_dashAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_dashOffset = value.toReal();
        update();
    });
    m_dashAnim->start();
}

void DependencyEdge::playAppearAnimation(int durationMs)
{
    stopAnimations();

    m_edgeOpacity = 0.0;
    m_drawProgress = 0.0;

    m_appearGroup = new QParallelAnimationGroup(this);

    auto* opacityAnim = new QPropertyAnimation(this, "edgeOpacity");
    opacityAnim->setDuration(durationMs);
    opacityAnim->setStartValue(0.0);
    opacityAnim->setEndValue(1.0);
    opacityAnim->setEasingCurve(QEasingCurve::OutQuad);

    auto* drawAnim = new QPropertyAnimation(this, "drawProgress");
    drawAnim->setDuration(durationMs);
    drawAnim->setStartValue(0.0);
    drawAnim->setEndValue(1.0);
    drawAnim->setEasingCurve(QEasingCurve::OutCubic);

    m_appearGroup->addAnimation(opacityAnim);
    m_appearGroup->addAnimation(drawAnim);

    connect(m_appearGroup, &QParallelAnimationGroup::finished, this, [this]() {
        m_edgeOpacity = 1.0;
        m_drawProgress = 1.0;
    });

    m_appearGroup->start();
}

void DependencyEdge::playDisappearAnimation(int durationMs)
{
    stopAnimations();

    m_disappearGroup = new QParallelAnimationGroup(this);

    auto* opacityAnim = new QPropertyAnimation(this, "edgeOpacity");
    opacityAnim->setDuration(durationMs);
    opacityAnim->setStartValue(m_edgeOpacity);
    opacityAnim->setEndValue(0.0);
    opacityAnim->setEasingCurve(QEasingCurve::InCubic);

    m_disappearGroup->addAnimation(opacityAnim);

    connect(m_disappearGroup, &QParallelAnimationGroup::finished, this, &DependencyEdge::disappearFinished);

    m_disappearGroup->start();
}

void DependencyEdge::stopAnimations()
{
    if (m_appearGroup) {
        m_appearGroup->stop();
        m_appearGroup->deleteLater();
        m_appearGroup = nullptr;
    }
    if (m_disappearGroup) {
        m_disappearGroup->stop();
        m_disappearGroup->deleteLater();
        m_disappearGroup = nullptr;
    }
}

QPainterPath DependencyEdge::curvePath() const
{
    if (!m_source || !m_target)
        return QPainterPath();

    QPointF p1 = m_source->pos();
    QPointF p2 = m_target->pos();

    if (qFuzzyCompare(p1.x(), p2.x()) && qFuzzyCompare(p1.y(), p2.y()))
        return QPainterPath();

    QPainterPath path;
    path.moveTo(p1);

    qreal dx = p2.x() - p1.x();
    qreal dy = p2.y() - p1.y();
    qreal dist = qSqrt(dx * dx + dy * dy);
    qreal offset = qMin(dist * 0.3, 80.0);

    QPointF cp1(p1.x() + dx * 0.33, p1.y() + offset);
    QPointF cp2(p1.x() + dx * 0.67, p2.y() - offset);

    path.cubicTo(cp1, cp2, p2);
    return path;
}

QColor DependencyEdge::edgeColor() const
{
    switch (m_type) {
    case Include: return QColor(100, 180, 255, 200);
    case Call:    return QColor(255, 180, 100, 200);
    case Inherit: return QColor(180, 255, 100, 200);
    default:      return QColor(150, 150, 150, 200);
    }
}
