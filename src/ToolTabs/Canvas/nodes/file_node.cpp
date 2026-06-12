#include "file_node.h"
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QFileIconProvider>
#include <QToolTip>

FileNode::FileNode(const QString& filePath, QGraphicsItem* parent)
    : QGraphicsObject(parent)
    , m_filePath(filePath)
    , m_fileName(QFileInfo(filePath).fileName())
    , m_suffix(QFileInfo(filePath).suffix())
{
    setFlag(QGraphicsItem::ItemIsMovable);
    setFlag(QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
    setZValue(1);
}

QRectF FileNode::boundingRect() const
{
    return QRectF(-WIDTH / 2, -HEIGHT / 2, WIDTH, HEIGHT);
}

void FileNode::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    painter->setRenderHint(QPainter::Antialiasing);
    QRectF rect = boundingRect();

    // Pulse glow
    if (m_pulseOpacity > 0.01) {
        QColor glowColor(70, 130, 180);
        glowColor.setAlphaF(m_pulseOpacity * 0.4);
        painter->setPen(Qt::NoPen);
        painter->setBrush(glowColor);
        painter->drawRoundedRect(rect.adjusted(-6, -6, 6, 6), RADIUS + 4, RADIUS + 4);
    }

    // Background
    QColor bgColor;
    if (m_state == Active)
        bgColor = QColor(55, 100, 150);
    else if (m_hovered)
        bgColor = QColor(55, 55, 70);
    else if (isSelected())
        bgColor = QColor(50, 50, 65);
    else
        bgColor = QColor(38, 38, 48);

    painter->setBrush(bgColor);
    painter->setPen(QPen(stateColor(), m_hovered ? 2.5 : 1.5));
    painter->drawRoundedRect(rect, RADIUS, RADIUS);

    // Icon
    QRectF iconRect(rect.left() + 8, rect.top() + (HEIGHT - ICON_SIZE) / 2, ICON_SIZE, ICON_SIZE);
    QFileIconProvider iconProvider;
    QIcon icon = iconProvider.icon(QFileInfo(m_filePath));
    icon.paint(painter, iconRect.toRect());

    // File name
    painter->setPen(Qt::white);
    QFont font = painter->font();
    font.setPointSize(9);
    painter->setFont(font);

    QRectF textRect(rect.left() + 34, rect.top(), rect.width() - 44, rect.height() - 14);
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, m_fileName);

    // Dependency count
    if (m_depCount > 0) {
        painter->setPen(QColor(140, 140, 160));
        QFont smallFont = painter->font();
        smallFont.setPointSize(7);
        painter->setFont(smallFont);
        QRectF countRect(rect.left() + 34, rect.bottom() - 14, rect.width() - 44, 12);
        painter->drawText(countRect, Qt::AlignLeft | Qt::AlignVCenter,
                          QString::number(m_depCount) + (m_depCount == 1 ? " dep" : " deps"));
    }
}

void FileNode::setState(State state)
{
    if (m_state == state) return;
    m_state = state;
    update();
}

void FileNode::setDependencyCount(int count)
{
    m_depCount = count;
    update();
}

void FileNode::startPulse()
{
    if (m_pulseAnim) {
        m_pulseAnim->stop();
        m_pulseAnim->deleteLater();
    }
    m_pulseAnim = new QPropertyAnimation(this, "pulseOpacity");
    m_pulseAnim->setDuration(600);
    m_pulseAnim->setStartValue(1.0);
    m_pulseAnim->setEndValue(0.0);
    m_pulseAnim->setEasingCurve(QEasingCurve::OutQuad);
    m_pulseAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

QColor FileNode::stateColor() const
{
    if (m_state == Active)   return QColor(70, 130, 180);
    if (m_state == Selected) return QColor(100, 100, 140);
    if (m_hovered)           return QColor(80, 80, 110);
    return QColor(55, 55, 65);
}

QString FileNode::tooltipText() const
{
    QString tip = m_filePath;
    if (m_depCount > 0)
        tip += "\n" + QString::number(m_depCount) + " dependencies";
    return tip;
}

void FileNode::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        emit clicked(m_filePath);
    QGraphicsObject::mousePressEvent(event);
}

void FileNode::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        emit doubleClicked(m_filePath);
    QGraphicsObject::mouseDoubleClickEvent(event);
}

void FileNode::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    Q_UNUSED(event)
    m_hovered = true;
    setToolTip(tooltipText());
    update();
}

void FileNode::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    Q_UNUSED(event)
    m_hovered = false;
    update();
}

QVariant FileNode::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemPositionHasChanged && scene())
        emit positionChanged();
    return QGraphicsObject::itemChange(change, value);
}
