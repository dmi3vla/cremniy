#include "connection_edge.h"
#include "../nodes/step_node.h"
#include <QPainter>
#include <QPen>
#include <QLineF>
#include <QtMath>
#include <QFontMetrics>

ConnectionEdge::ConnectionEdge(StepNode* source, StepNode* target, const QString& relationLabel, QGraphicsItem* parent)
    : QGraphicsObject(parent)
    , m_source(source)
    , m_target(target)
    , m_relationLabel(relationLabel)
{
    setZValue(0);

    if (m_source)
        connect(m_source, &StepNode::positionChanged, this, &ConnectionEdge::updatePosition);
    if (m_target)
        connect(m_target, &StepNode::positionChanged, this, &ConnectionEdge::updatePosition);
}

QRectF ConnectionEdge::boundingRect() const
{
    if (!m_source || !m_target)
        return QRectF();

    QPainterPath path = curvePath();
    return path.boundingRect().adjusted(-20, -20, 20, 20);
}

void ConnectionEdge::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
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
        QPointF p1 = m_source->scenePos();
        QPointF p2 = m_target->scenePos();
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

    QPen pen(color, 2.0);
    painter->setPen(pen);
    painter->drawPath(drawPath);

    // Arrow at the end (only when mostly drawn)
    if (m_drawProgress >= 0.95) {
        QPointF end = path.pointAtPercent(1.0);
        QPointF before = path.pointAtPercent(0.95);
        double angle = std::atan2(end.y() - before.y(), end.x() - before.x());

        QPointF arrowP1 = end - QPointF(qSin(angle + M_PI / 3) * 12, qCos(angle + M_PI / 3) * 12);
        QPointF arrowP2 = end - QPointF(qSin(angle + M_PI - M_PI / 3) * 12, qCos(angle + M_PI - M_PI / 3) * 12);

        QPolygonF arrowHead;
        arrowHead << end << arrowP1 << arrowP2;
        painter->setBrush(color);
        painter->setPen(Qt::NoPen);
        painter->drawPolygon(arrowHead);
    }

    // Draw relation label at center (only when fully drawn)
    if (m_drawProgress >= 0.95 && !m_relationLabel.isEmpty()) {
        QPointF center = path.pointAtPercent(0.5);
        
        // Draw label background
        QFont font = painter->font();
        font.setPointSize(8);
        painter->setFont(font);
        QFontMetrics fm(font);
        QRect textRect = fm.boundingRect(m_relationLabel);
        
        QRectF bgRect(center.x() - textRect.width() / 2 - 6,
                      center.y() - textRect.height() / 2 - 4,
                      textRect.width() + 12,
                      textRect.height() + 8);
        
        painter->setBrush(QColor(30, 30, 40, 230));
        painter->setPen(QPen(color, 1.0));
        painter->drawRoundedRect(bgRect, 4, 4);
        
        // Draw label text
        painter->setPen(Qt::white);
        painter->drawText(bgRect, Qt::AlignCenter, m_relationLabel);
    }
}

void ConnectionEdge::updatePosition()
{
    if (m_source && m_target) {
        prepareGeometryChange();
        update();
    }
}

void ConnectionEdge::playAppearAnimation(int durationMs)
{
    if (m_appearGroup) {
        m_appearGroup->stop();
        delete m_appearGroup;
    }

    m_drawProgress = 0.0;
    m_edgeOpacity = 0.0;

    m_appearGroup = new QParallelAnimationGroup(this);

    QPropertyAnimation* progressAnim = new QPropertyAnimation(this, "drawProgress");
    progressAnim->setDuration(durationMs);
    progressAnim->setStartValue(0.0);
    progressAnim->setEndValue(1.0);
    progressAnim->setEasingCurve(QEasingCurve::OutCubic);

    QPropertyAnimation* opacityAnim = new QPropertyAnimation(this, "edgeOpacity");
    opacityAnim->setDuration(durationMs);
    opacityAnim->setStartValue(0.0);
    opacityAnim->setEndValue(1.0);
    opacityAnim->setEasingCurve(QEasingCurve::OutCubic);

    m_appearGroup->addAnimation(progressAnim);
    m_appearGroup->addAnimation(opacityAnim);
    m_appearGroup->start(QAbstractAnimation::DeleteWhenStopped);
}

void ConnectionEdge::playDisappearAnimation(int durationMs)
{
    if (m_disappearGroup) {
        m_disappearGroup->stop();
        delete m_disappearGroup;
    }

    m_disappearGroup = new QParallelAnimationGroup(this);

    QPropertyAnimation* progressAnim = new QPropertyAnimation(this, "drawProgress");
    progressAnim->setDuration(durationMs);
    progressAnim->setStartValue(1.0);
    progressAnim->setEndValue(0.0);
    progressAnim->setEasingCurve(QEasingCurve::InCubic);

    QPropertyAnimation* opacityAnim = new QPropertyAnimation(this, "edgeOpacity");
    opacityAnim->setDuration(durationMs);
    opacityAnim->setStartValue(1.0);
    opacityAnim->setEndValue(0.0);
    opacityAnim->setEasingCurve(QEasingCurve::InCubic);

    m_disappearGroup->addAnimation(progressAnim);
    m_disappearGroup->addAnimation(opacityAnim);

    connect(m_disappearGroup, &QParallelAnimationGroup::finished, this, &ConnectionEdge::disappearFinished);
    m_disappearGroup->start(QAbstractAnimation::DeleteWhenStopped);
}

void ConnectionEdge::stopAnimations()
{
    if (m_appearGroup) {
        m_appearGroup->stop();
        delete m_appearGroup;
        m_appearGroup = nullptr;
    }
    if (m_disappearGroup) {
        m_disappearGroup->stop();
        delete m_disappearGroup;
        m_disappearGroup = nullptr;
    }
}

QColor ConnectionEdge::edgeColor() const
{
    return QColor(100, 150, 200);
}

QPainterPath ConnectionEdge::curvePath() const
{
    if (!m_source || !m_target)
        return QPainterPath();

    QPointF p1 = m_source->scenePos();
    QPointF p2 = m_target->scenePos();

    QPainterPath path;
    path.moveTo(p1);

    // Create cubic bezier curve
    QPointF cp1(p1.x() + (p2.x() - p1.x()) * 0.33,
                 p1.y() + qMin((p2.x() - p1.x()) * 0.3, 80.0));
    QPointF cp2(p1.x() + (p2.x() - p1.x()) * 0.67,
                 p2.y() - qMin((p2.x() - p1.x()) * 0.3, 80.0));

    path.cubicTo(cp1, cp2, p2);
    return path;
}
