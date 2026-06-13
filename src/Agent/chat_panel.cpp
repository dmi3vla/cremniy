#include "chat_panel.h"
#include <QKeyEvent>
#include <QScrollBar>
#include <QHBoxLayout>

ChatPanel::ChatPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Header
    auto* header = new QWidget(this);
    header->setFixedHeight(32);
    header->setStyleSheet("background: #1e1e28; border-bottom: 1px solid #333;");
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 0, 12, 0);
    auto* titleLabel = new QLabel("AI Assistant", header);
    titleLabel->setStyleSheet("color: #aaa; font-weight: bold; font-size: 12px; background: transparent;");
    headerLayout->addWidget(titleLabel);
    mainLayout->addWidget(header);

    // Messages area
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameStyle(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setStyleSheet("background: #252530;");

    m_messagesContainer = new QWidget();
    m_messagesLayout = new QVBoxLayout(m_messagesContainer);
    m_messagesLayout->setContentsMargins(8, 8, 8, 8);
    m_messagesLayout->setSpacing(4);
    m_messagesLayout->addStretch();
    m_scrollArea->setWidget(m_messagesContainer);

    mainLayout->addWidget(m_scrollArea, 1);

    // Input area
    auto* inputArea = new QWidget(this);
    inputArea->setStyleSheet("background: #1e1e28; border-top: 1px solid #333;");
    auto* inputLayout = new QHBoxLayout(inputArea);
    inputLayout->setContentsMargins(8, 8, 8, 8);
    inputLayout->setSpacing(4);

    m_input = new QLineEdit(inputArea);
    m_input->setPlaceholderText("Type a message...");
    m_input->setStyleSheet(
        "QLineEdit { background: #2a2a35; color: #ddd; border: 1px solid #444; "
        "border-radius: 6px; padding: 8px 12px; font-size: 13px; }"
        "QLineEdit:focus { border-color: #5a8abf; }"
    );
    m_input->installEventFilter(this);

    m_sendBtn = new QPushButton("Send", inputArea);
    m_sendBtn->setStyleSheet(
        "QPushButton { background: #3a5a7a; color: #ddd; border: none; "
        "border-radius: 6px; padding: 8px 16px; font-weight: bold; }"
        "QPushButton:hover { background: #4a6a8a; }"
        "QPushButton:pressed { background: #2a4a6a; }"
    );
    m_sendBtn->setFixedWidth(60);

    inputLayout->addWidget(m_input, 1);
    inputLayout->addWidget(m_sendBtn);

    mainLayout->addWidget(inputArea);

    connect(m_sendBtn, &QPushButton::clicked, this, &ChatPanel::onSendMessage);
}

void ChatPanel::addMessage(const QString& text, ChatMessage::Role role)
{
    auto* msg = new ChatMessage(text, role, m_messagesContainer);
    m_messages.append(msg);
    m_messagesLayout->insertWidget(m_messagesLayout->count() - 1, msg);
    connect(msg, &ChatMessage::sizeChanged, this, &ChatPanel::sizeChanged);
    scrollToBottom();
}

void ChatPanel::appendToLastMessage(const QString& text)
{
    if (!m_messages.isEmpty()) {
        m_messages.last()->appendText(text);
        scrollToBottom();
    }
}

void ChatPanel::clearMessages()
{
    qDeleteAll(m_messages);
    m_messages.clear();
}

ChatMessage* ChatPanel::lastMessage() const
{
    return m_messages.isEmpty() ? nullptr : m_messages.last();
}

void ChatPanel::onSendMessage()
{
    QString text = m_input->text().trimmed();
    if (text.isEmpty()) return;

    m_input->clear();
    addMessage(text, ChatMessage::User);
    emit messageSent(text);
}

bool ChatPanel::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_input && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (!(keyEvent->modifiers() & Qt::ShiftModifier)) {
                onSendMessage();
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void ChatPanel::scrollToBottom()
{
    QScrollBar* sb = m_scrollArea->verticalScrollBar();
    sb->setValue(sb->maximum());
}
