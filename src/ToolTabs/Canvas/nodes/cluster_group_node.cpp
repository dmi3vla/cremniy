#include "cluster_group_node.h"
#include "step_node.h"
#include <QStyleOptionGraphicsItem>
#include <QFontMetrics>

ClusterGroupNode::ClusterGroupNode(const QString& clusterId, const QString& title, const QString& color, QGraphicsItem* parent)
    : QGraphicsObject(parent)
    , m_clusterId(clusterId)
    , m_title(title)
    , m_color(color)
{
    setFlag(QGraphicsItem::ItemIsMovable);
    setFlag(QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges);
    setZValue(1);
    m_bounds = QRectF(0, 0, 300, 200);
}

QRectF ClusterGroupNode::boundingRect() const
{
    return m_bounds;
}

void ClusterGroupNode::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setOpacity(m_appearOpacity);

    // Parse color
    QColor frameColor(m_color);
    if (!frameColor.isValid()) {
        frameColor = QColor(100, 150, 200);
    }

    // Draw background with slight transparency
    QColor bgColor = frameColor;
    bgColor.setAlphaF(0.1);
    painter->setBrush(bgColor);
    painter->setPen(QPen(frameColor, 2.0));
    painter->drawRoundedRect(m_bounds, CORNER_RADIUS, CORNER_RADIUS);

    // Draw header background
    QRectF headerRect(m_bounds.left(), m_bounds.top(), m_bounds.width(), HEADER_HEIGHT);
    QColor headerBgColor = frameColor;
    headerBgColor.setAlphaF(0.2);
    painter->setBrush(headerBgColor);
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(headerRect, CORNER_RADIUS, CORNER_RADIUS);

    // Draw header bottom line
    painter->setPen(QPen(frameColor, 2.0));
    QLineF bottomLine(headerRect.bottomLeft(), headerRect.bottomRight());
    painter->drawLine(bottomLine);

    // Draw title
    painter->setPen(Qt::white);
    QFont titleFont = painter->font();
    titleFont.setPointSize(10);
    titleFont.setBold(true);
    painter->setFont(titleFont);
    QRectF titleRect(headerRect.adjusted(PADDING, 0, -PADDING, 0));
    painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, m_title);

    painter->restore();
}

void ClusterGroupNode::addChild(StepNode* child)
{
    if (!m_children.contains(child)) {
        m_children.append(child);
        child->setParentItem(this);
        updateBoundsFromChildren();
    }
}

void ClusterGroupNode::removeChild(StepNode* child)
{
    if (m_children.removeOne(child)) {
        child->setParentItem(nullptr);
        updateBoundsFromChildren();
    }
}

void ClusterGroupNode::updateBoundsFromChildren()
{
    if (m_children.isEmpty()) {
        m_bounds = QRectF(0, 0, 300, HEADER_HEIGHT + PADDING * 2);
        update();
        return;
    }

    // Calculate bounding box of all children
    QRectF childrenBounds;
    for (StepNode* child : m_children) {
        QRectF childBounds = child->boundingRect().translated(child->pos());
        if (childrenBounds.isEmpty()) {
            childrenBounds = childBounds;
        } else {
            childrenBounds = childrenBounds.united(childBounds);
        }
    }

    // Add padding and header height
    qreal width = childrenBounds.width() + PADDING * 2;
    qreal height = childrenBounds.height() + HEADER_HEIGHT + PADDING * 2;

    // Ensure minimum size
    width = qMax(width, 300.0);
    height = qMax(height, HEADER_HEIGHT + PADDING * 2);

    m_bounds = QRectF(0, 0, width, height);
    update();
}

void ClusterGroupNode::playAppearAnimation(int durationMs)
{
    if (m_appearGroup) {
        m_appearGroup->stop();
        m_appearGroup->deleteLater();
    }

    m_appearOpacity = 0.0;

    m_appearGroup = new QParallelAnimationGroup(this);

    auto* opacityAnim = new QPropertyAnimation(this, "appearOpacity");
    opacityAnim->setDuration(durationMs);
    opacityAnim->setStartValue(0.0);
    opacityAnim->setEndValue(1.0);
    opacityAnim->setEasingCurve(QEasingCurve::OutCubic);

    m_appearGroup->addAnimation(opacityAnim);
    m_appearGroup->start(QAbstractAnimation::DeleteWhenStopped);
}

QVariant ClusterGroupNode::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemPositionHasChanged && scene())
        emit positionChanged();
    return QGraphicsObject::itemChange(change, value);
}
