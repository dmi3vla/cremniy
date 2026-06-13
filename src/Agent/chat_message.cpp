#include "chat_message.h"
#include "markdown_highlighter.h"
#include <QHBoxLayout>
#include <QMouseEvent>

ChatMessage::ChatMessage(const QString& text, Role role, QWidget* parent)
    : QWidget(parent)
    , m_text(text)
    , m_role(role)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 4, 8, 4);
    mainLayout->setSpacing(0);

    m_container = new QWidget(this);
    auto* containerLayout = new QVBoxLayout(m_container);
    containerLayout->setContentsMargins(12, 8, 12, 8);
    containerLayout->setSpacing(4);

    // Header (role name)
    m_headerLabel = new QLabel(roleName(), this);
    m_headerLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 11px; background: transparent;")
        .arg(roleColor()));
    containerLayout->addWidget(m_headerLabel);

    // Message body
    m_textBrowser = new QTextBrowser(this);
    m_textBrowser->setOpenExternalLinks(true);
    m_textBrowser->setReadOnly(true);
    m_textBrowser->setFrameStyle(QFrame::NoFrame);
    m_textBrowser->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_textBrowser->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_textBrowser->document()->setDefaultStyleSheet(
        "body { color: #ddd; font-family: sans-serif; font-size: 13px; }"
        "code { background: #2d2d37; color: #dcb478; padding: 2px 4px; border-radius: 3px; font-family: monospace; }"
        "pre { background: #23232d; color: #c8c8d2; padding: 8px; border-radius: 4px; font-family: monospace; }"
        "a { color: #64b4ff; }"
    );

    auto* highlighter = new MarkdownHighlighter(m_textBrowser->document());

    containerLayout->addWidget(m_textBrowser);

    mainLayout->addWidget(m_container);

    // Alignment: user right, assistant left
    if (m_role == User) {
        mainLayout->setAlignment(m_container, Qt::AlignRight);
    } else {
        mainLayout->setAlignment(m_container, Qt::AlignLeft);
    }

    updateStyle();
    setText(text);
}

void ChatMessage::setText(const QString& text)
{
    m_text = text;
    m_textBrowser->setMarkdown(text);

    // Auto-resize to content
    QSize docSize = m_textBrowser->document()->size().toSize();
    int h = qMin(docSize.height() + 4, 400);
    m_textBrowser->setFixedHeight(qMax(h, 30));
}

void ChatMessage::appendText(const QString& text)
{
    m_text += text;
    setText(m_text);
}

void ChatMessage::setExpanded(bool expanded)
{
    m_expanded = expanded;
    m_textBrowser->setVisible(expanded);
    if (!expanded)
        m_textBrowser->setFixedHeight(0);
}

void ChatMessage::updateStyle()
{
    QString bgColor;
    QString borderColor;

    switch (m_role) {
    case User:
        bgColor = "#2a3a4a";
        borderColor = "#3a5a7a";
        break;
    case Assistant:
        bgColor = "#2a2a35";
        borderColor = "#444";
        break;
    case System:
        bgColor = "#3a2a2a";
        borderColor = "#7a3a3a";
        break;
    case ToolCall:
        bgColor = "#2a3a2a";
        borderColor = "#3a7a3a";
        break;
    case ToolResult:
        bgColor = "#3a3a2a";
        borderColor = "#7a7a3a";
        break;
    }

    m_container->setStyleSheet(QString(
        "background: %1; border: 1px solid %2; border-radius: 8px;")
        .arg(bgColor, borderColor));
}

QString ChatMessage::roleColor() const
{
    switch (m_role) {
    case User:      return "#64b4ff";
    case Assistant:  return "#a0a0c0";
    case System:    return "#ff8080";
    case ToolCall:  return "#80cc80";
    case ToolResult: return "#cccc80";
    }
    return "#ccc";
}

QString ChatMessage::roleName() const
{
    switch (m_role) {
    case User:      return "You";
    case Assistant:  return "Assistant";
    case System:    return "System";
    case ToolCall:  return "Tool Call";
    case ToolResult: return "Tool Result";
    }
    return "";
}
