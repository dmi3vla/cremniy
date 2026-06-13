#ifndef CONVERSATION_HISTORY_H
#define CONVERSATION_HISTORY_H

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

/**
 * @brief Per-project conversation history with JSON persistence.
 *
 * Stores messages in Anthropic Messages API format:
 *   [ { "role": "user"|"assistant", "content": ... }, ... ]
 *
 * Persisted to:  <projectRoot>/.cremniy/chat_history.json
 */
class ConversationHistory : public QObject
{
    Q_OBJECT

public:
    explicit ConversationHistory(QObject* parent = nullptr);

    /** Set project root and load existing history from disk. */
    void setProjectRoot(const QString& projectRoot);
    QString projectRoot() const { return m_projectRoot; }

    /** Append a user text message. */
    void addUserMessage(const QString& text);

    /** Append an assistant text message. */
    void addAssistantMessage(const QString& text);

    /**
     * @brief Append an assistant message with mixed content blocks.
     * @param contentBlocks  Array of content block objects
     *        (text, tool_use, etc.)
     */
    void addAssistantBlocks(const QJsonArray& contentBlocks);

    /**
     * @brief Append a tool_result message.
     * @param toolUseId   The tool_use id this result corresponds to.
     * @param content     The result content (string or array of content blocks).
     * @param isError     Whether the tool call failed.
     */
    void addToolResult(const QString& toolUseId,
                       const QString& content,
                       bool isError = false);

    /** Get the full messages array for the API request. */
    QJsonArray messages() const { return m_messages; }

    /** Number of messages. */
    int count() const { return m_messages.size(); }

    /** Clear all messages (does NOT delete the file). */
    void clear();

    /** Save to disk. */
    bool save() const;

    /** Load from disk. Returns false if file doesn't exist or is invalid. */
    bool load();

signals:
    void historyChanged();

private:
    QString historyFilePath() const;

    QString m_projectRoot;
    QJsonArray m_messages;
};

#endif // CONVERSATION_HISTORY_H
