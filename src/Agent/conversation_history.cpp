#include "conversation_history.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>

ConversationHistory::ConversationHistory(QObject* parent)
    : QObject(parent)
{
}

void ConversationHistory::setProjectRoot(const QString& projectRoot)
{
    m_projectRoot = projectRoot;
    m_messages = QJsonArray();
    load();
}

void ConversationHistory::addUserMessage(const QString& text)
{
    QJsonObject msg;
    msg["role"] = "user";
    msg["content"] = text;
    m_messages.append(msg);
    save();
    emit historyChanged();
}

void ConversationHistory::addAssistantMessage(const QString& text)
{
    QJsonObject msg;
    msg["role"] = "assistant";
    msg["content"] = text;
    m_messages.append(msg);
    save();
    emit historyChanged();
}

void ConversationHistory::addAssistantBlocks(const QJsonArray& contentBlocks)
{
    QJsonObject msg;
    msg["role"] = "assistant";
    msg["content"] = contentBlocks;
    m_messages.append(msg);
    save();
    emit historyChanged();
}

void ConversationHistory::addToolResult(const QString& toolUseId,
                                         const QString& content,
                                         bool isError)
{
    QJsonObject msg;
    msg["role"] = "user";

    QJsonObject toolResult;
    toolResult["type"] = "tool_result";
    toolResult["tool_use_id"] = toolUseId;
    toolResult["content"] = content;
    if (isError) {
        toolResult["is_error"] = true;
    }

    QJsonArray arr;
    arr.append(toolResult);
    msg["content"] = arr;

    m_messages.append(msg);
    save();
    emit historyChanged();
}

void ConversationHistory::clear()
{
    m_messages = QJsonArray();
    emit historyChanged();
}

QString ConversationHistory::historyFilePath() const
{
    if (m_projectRoot.isEmpty()) return {};
    return m_projectRoot + "/.cremniy/chat_history.json";
}

bool ConversationHistory::save() const
{
    const QString path = historyFilePath();
    if (path.isEmpty()) return false;

    // Ensure .cremniy directory exists
    QDir dir(m_projectRoot + "/.cremniy");
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    QJsonDocument doc(m_messages);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool ConversationHistory::load()
{
    const QString path = historyFilePath();
    if (path.isEmpty()) return false;

    QFile file(path);
    if (!file.exists()) return false;
    if (!file.open(QIODevice::ReadOnly)) return false;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    file.close();

    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        return false;
    }

    m_messages = doc.array();
    emit historyChanged();
    return true;
}
