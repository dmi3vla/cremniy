#ifndef FILE_NODE_H
#define FILE_NODE_H

#include <QGraphicsItem>
#include <QGraphicsObject>
#include <QPainter>
#include <QString>
#include <QFileInfo>
#include <QPropertyAnimation>

class FileNode : public QGraphicsObject
{
    Q_OBJECT
    Q_PROPERTY(qreal pulseOpacity READ pulseOpacity WRITE setPulseOpacity)

public:
    enum State { Normal, Selected, Active };

    explicit FileNode(const QString& filePath, QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    QString filePath() const { return m_filePath; }
    QString fileName() const { return m_fileName; }
    void setState(State state);
    State state() const { return m_state; }

    void setDependencyCount(int count);
    int dependencyCount() const { return m_depCount; }

    void startPulse();
    qreal pulseOpacity() const { return m_pulseOpacity; }
    void setPulseOpacity(qreal opacity) { m_pulseOpacity = opacity; update(); }

signals:
    void clicked(const QString& filePath);
    void doubleClicked(const QString& filePath);
    void positionChanged();

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private:
    QString m_filePath;
    QString m_fileName;
    QString m_suffix;
    State m_state = Normal;
    int m_depCount = 0;
    bool m_hovered = false;
    qreal m_pulseOpacity = 0.0;
    QPropertyAnimation* m_pulseAnim = nullptr;
    static constexpr qreal WIDTH = 160;
    static constexpr qreal HEIGHT = 50;
    static constexpr qreal ICON_SIZE = 20;
    static constexpr qreal RADIUS = 8;

    QColor stateColor() const;
    QString tooltipText() const;
};

#endif // FILE_NODE_H
