#include "step_node.h"
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QFontMetrics>

StepNode::StepNode(const QString& stepId, const QString& title, const QString& codeSnippet,
                   const QString& filePath, int lineNumber, QGraphicsItem* parent)
    : QGraphicsObject(parent)
    , m_stepId(stepId)
    , m_title(title)
    , m_codeSnippet(codeSnippet)
    , m_filePath(filePath)
    , m_lineNumber(lineNumber)
{
    setFlag(QGraphicsItem::ItemIsMovable);
    setFlag(QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
    setZValue(2);
}

QRectF StepNode::boundingRect() const
{
    return QRectF(-WIDTH / 2, -HEIGHT / 2, WIDTH, HEIGHT);
}

void StepNode::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setOpacity(m_appearOpacity);
    painter->scale(m_appearScale, m_appearScale);
    QRectF rect = boundingRect();

    // Background
    QColor bgColor;
    if (m_hovered)
        bgColor = QColor(55, 55, 70);
    else if (isSelected())
        bgColor = QColor(50, 50, 65);
    else
        bgColor = QColor(38, 38, 48);

    painter->setBrush(bgColor);
    painter->setPen(QPen(m_hovered ? QColor(100, 150, 200) : QColor(70, 70, 85), m_hovered ? 2.0 : 1.5));
    painter->drawRoundedRect(rect, RADIUS, RADIUS);

    // Step ID (small, top left)
    painter->setPen(QColor(140, 140, 160));
    QFont idFont = painter->font();
    idFont.setPointSize(7);
    painter->setFont(idFont);
    QRectF idRect(rect.left() + PADDING, rect.top() + PADDING, rect.width() - 2 * PADDING, 14);
    painter->drawText(idRect, Qt::AlignLeft | Qt::AlignVCenter, m_stepId);

    // Title
    painter->setPen(Qt::white);
    QFont titleFont = painter->font();
    titleFont.setPointSize(9);
    titleFont.setBold(true);
    painter->setFont(titleFont);
    QRectF titleRect(rect.left() + PADDING, rect.top() + PADDING + 16, rect.width() - 2 * PADDING, 18);
    QString elidedTitle = QFontMetrics(titleFont).elidedText(m_title, Qt::ElideRight, int(rect.width() - 2 * PADDING));
    painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, elidedTitle);

    // Code snippet (truncated)
    painter->setPen(QColor(180, 180, 200));
    QFont codeFont = painter->font();
    codeFont.setPointSize(8);
    codeFont.setBold(false);
    painter->setFont(codeFont);
    QRectF codeRect(rect.left() + PADDING, rect.top() + PADDING + 36, rect.width() - 2 * PADDING, 32);
    QString elidedCode = QFontMetrics(codeFont).elidedText(m_codeSnippet, Qt::ElideRight, int(rect.width() - 2 * PADDING));
    painter->drawText(codeRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, elidedCode);

    painter->restore();
}

void StepNode::playAppearAnimation(int durationMs)
{
    if (m_appearGroup) {
        m_appearGroup->stop();
        delete m_appearGroup;
    }

    m_appearOpacity = 0.0;
    m_appearScale = 0.8;

    m_appearGroup = new QParallelAnimationGroup(this);

    QPropertyAnimation* opacityAnim = new QPropertyAnimation(this, "appearOpacity");
    opacityAnim->setDuration(durationMs);
    opacityAnim->setStartValue(0.0);
    opacityAnim->setEndValue(1.0);
    opacityAnim->setEasingCurve(QEasingCurve::OutCubic);

    QPropertyAnimation* scaleAnim = new QPropertyAnimation(this, "appearScale");
    scaleAnim->setDuration(durationMs);
    scaleAnim->setStartValue(0.8);
    scaleAnim->setEndValue(1.0);
    scaleAnim->setEasingCurve(QEasingCurve::OutBack);

    m_appearGroup->addAnimation(opacityAnim);
    m_appearGroup->addAnimation(scaleAnim);
    m_appearGroup->start(QAbstractAnimation::DeleteWhenStopped);
}

void StepNode::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit stepClicked(m_filePath, m_lineNumber);
    }
    QGraphicsObject::mousePressEvent(event);
}

void StepNode::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    m_hovered = true;
    update();
    QGraphicsObject::hoverEnterEvent(event);
}

void StepNode::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    m_hovered = false;
    update();
    QGraphicsObject::hoverLeaveEvent(event);
}

QVariant StepNode::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemPositionHasChanged && scene())
        emit positionChanged();
    return QGraphicsObject::itemChange(change, value);
}
