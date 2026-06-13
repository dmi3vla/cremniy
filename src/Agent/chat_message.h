#ifndef CHAT_MESSAGE_H
#define CHAT_MESSAGE_H

#include <QWidget>
#include <QLabel>
#include <QTextBrowser>
#include <QVBoxLayout>

class ChatMessage : public QWidget
{
    Q_OBJECT

public:
    enum Role { User, Assistant, System, ToolCall, ToolResult };

    explicit ChatMessage(const QString& text, Role role, QWidget* parent = nullptr);

    void setText(const QString& text);
    QString text() const { return m_text; }
    Role role() const { return m_role; }

    void setExpanded(bool expanded);
    bool isExpanded() const { return m_expanded; }

    void appendText(const QString& text);

signals:
    void sizeChanged();

private:
    QString m_text;
    Role m_role;
    bool m_expanded = true;
    QTextBrowser* m_textBrowser = nullptr;
    QLabel* m_headerLabel = nullptr;
    QWidget* m_container = nullptr;

    void updateStyle();
    QString roleColor() const;
    QString roleName() const;
};

#endif // CHAT_MESSAGE_H
