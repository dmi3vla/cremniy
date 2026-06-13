#include "file_node.h"
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QFileIconProvider>
#include <QToolTip>
#include <QTextStream>
#include <QDateTime>
#include <QMenu>
#include <QClipboard>
#include <QApplication>
#include <QLineEdit>
#include <QDockWidget>

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
    QString tip = m_filePath + "\n";

    // File preview (first 8 lines)
    QFile file(m_filePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QStringList lines;
        for (int i = 0; i < 8 && !in.atEnd(); ++i)
            lines.append(in.readLine());
        file.close();
        if (!lines.isEmpty()) {
            tip += "\n--- Preview ---\n";
            tip += lines.join("\n");
            tip += "\n";
        }
    }

    // Dependencies
    if (!m_outgoingDeps.isEmpty()) {
        tip += "\nIncludes (" + QString::number(m_outgoingDeps.size()) + "):";
        for (const QString& dep : m_outgoingDeps)
            tip += "\n  " + QFileInfo(dep).fileName();
    }
    if (!m_incomingDeps.isEmpty()) {
        tip += "\nIncluded by (" + QString::number(m_incomingDeps.size()) + "):";
        for (const QString& dep : m_incomingDeps)
            tip += "\n  " + QFileInfo(dep).fileName();
    }

    // Last modified
    QFileInfo fi(m_filePath);
    tip += "\n\nModified: " + fi.lastModified().toString("yyyy-MM-dd HH:mm:ss");

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

void FileNode::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
    QMenu menu;

    menu.addAction("Open in Editor", [this]() {
        emit openInEditor(m_filePath);
    });

    menu.addAction("Open in HEX Editor", [this]() {
        emit openInHexEditor(m_filePath);
    });

    menu.addSeparator();

    menu.addAction("Show Dependencies", [this]() {
        emit clicked(m_filePath);
    });

    menu.addAction("Hide Unconnected", [this]() {
        emit hideUnconnected(m_filePath);
    });

    menu.addAction("Ask AI Agent", [this]() {
        QWidget* activeWin = nullptr;
        for (QWidget* w : QApplication::topLevelWidgets()) {
            if (w->inherits("QMainWindow")) {
                activeWin = w;
                break;
            }
        }
        if (activeWin) {
            QDockWidget* chatDock = nullptr;
            for (QDockWidget* dock : activeWin->findChildren<QDockWidget*>()) {
                if (dock->widget() && (dock->widget()->inherits("ChatPanel") || dock->widget()->metaObject()->className() == QString("ChatPanel"))) {
                    chatDock = dock;
                    break;
                }
            }
            if (chatDock) {
                chatDock->setVisible(true);
                chatDock->raise();
                QWidget* chatPanel = chatDock->widget();
                if (chatPanel) {
                    QLineEdit* input = chatPanel->findChild<QLineEdit*>();
                    if (input) {
                        input->setText(QString("Tell me about the file: %1").arg(QFileInfo(m_filePath).fileName()));
                        input->setFocus();
                    }
                }
            }
        }
    });

    menu.addSeparator();

    menu.addAction("Copy Path", [this]() {
        QApplication::clipboard()->setText(m_filePath);
    });

    menu.exec(event->screenPos());
}

QVariant FileNode::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemPositionHasChanged && scene())
        emit positionChanged();
    return QGraphicsObject::itemChange(change, value);
}
