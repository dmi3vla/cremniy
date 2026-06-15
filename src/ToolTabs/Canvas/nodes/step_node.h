#ifndef STEP_NODE_H
#define STEP_NODE_H

#include <QGraphicsItem>
#include <QGraphicsObject>
#include <QPainter>
#include <QString>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>

class StepNode : public QGraphicsObject
{
    Q_OBJECT
    Q_PROPERTY(qreal appearOpacity READ appearOpacity WRITE setAppearOpacity)
    Q_PROPERTY(qreal appearScale READ appearScale WRITE setAppearScale)

public:
    explicit StepNode(const QString& stepId, const QString& title, const QString& codeSnippet,
                     const QString& filePath, int lineNumber, QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    QString stepId() const { return m_stepId; }
    QString title() const { return m_title; }
    QString codeSnippet() const { return m_codeSnippet; }
    QString filePath() const { return m_filePath; }
    int lineNumber() const { return m_lineNumber; }
    bool isStale() const { return m_stale; }
    void setStale(bool stale) { m_stale = stale; update(); }

    void playAppearAnimation(int durationMs = 300);
    qreal appearOpacity() const { return m_appearOpacity; }
    void setAppearOpacity(qreal opacity) { m_appearOpacity = opacity; update(); }
    qreal appearScale() const { return m_appearScale; }
    void setAppearScale(qreal scale) { m_appearScale = scale; update(); }

signals:
    void stepClicked(const QString& filePath, int lineNumber);
    void positionChanged();

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private:
    QString m_stepId;
    QString m_title;
    QString m_codeSnippet;
    QString m_filePath;
    int m_lineNumber;
    bool m_hovered = false;
    bool m_stale = false;
    qreal m_appearOpacity = 1.0;
    qreal m_appearScale = 1.0;
    QParallelAnimationGroup* m_appearGroup = nullptr;

    static constexpr qreal WIDTH = 200;
    static constexpr qreal HEIGHT = 80;
    static constexpr qreal RADIUS = 8;
    static constexpr qreal PADDING = 12;
};

#endif // STEP_NODE_H
