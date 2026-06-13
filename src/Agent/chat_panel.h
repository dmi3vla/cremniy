#ifndef CHAT_PANEL_H
#define CHAT_PANEL_H

#include <QWidget>
#include <QScrollArea>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QList>
#include "chat_message.h"

class ChatPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ChatPanel(QWidget* parent = nullptr);

    void addMessage(const QString& text, ChatMessage::Role role);
    void appendToLastMessage(const QString& text);
    void clearMessages();

    ChatMessage* lastMessage() const;

signals:
    void messageSent(const QString& text);
    void sizeChanged();

public slots:
    void onSendMessage();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    QScrollArea* m_scrollArea;
    QWidget* m_messagesContainer;
    QVBoxLayout* m_messagesLayout;
    QLineEdit* m_input;
    QPushButton* m_sendBtn;
    QList<ChatMessage*> m_messages;

    void scrollToBottom();
};

#endif // CHAT_PANEL_H
