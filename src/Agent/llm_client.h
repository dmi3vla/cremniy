#ifndef LLM_CLIENT_H
#define LLM_CLIENT_H

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>

class QNetworkAccessManager;
class QNetworkReply;

/**
 * @brief Anthropic Messages API client with SSE streaming.
 *
 * Sends requests to the Anthropic Messages API and streams back
 * tokens via Server-Sent Events (SSE).  Emits streamToken() for
 * each content_block_delta, streamFinished() when the response
 * is complete, and errorOccurred() on failure.
 */
class LLMClient : public QObject
{
    Q_OBJECT

public:
    explicit LLMClient(QObject* parent = nullptr);
    ~LLMClient() override;

    /**
     * @brief Send a Messages API request with streaming.
     * @param messages  JSON array of {role, content} objects.
     * @param tools     Optional JSON array of tool definitions.
     *
     * Uses apiKey / model / systemPrompt from AppSettings.
     */
    void sendMessage(const QJsonArray& messages,
                     const QJsonArray& tools = {});

    /** Abort the current streaming request (if any). */
    void abort();

    /** True while a streaming request is in progress. */
    bool isBusy() const;

signals:
    /** Emitted for each text delta token from the stream. */
    void streamToken(const QString& token);

    /**
     * @brief Emitted when a complete content block finishes.
     * @param block  The completed content block JSON object.
     *               For text: {"type":"text","text":"..."}
     *               For tool_use: {"type":"tool_use","id":"...","name":"...","input":{...}}
     */
    void contentBlockFinished(const QJsonObject& block);

    /** Emitted when the entire response stream ends successfully.
     *  @param stopReason  e.g. "end_turn", "tool_use", "max_tokens"
     */
    void streamFinished(const QString& stopReason);

    /** Emitted on any error (network, API, parse). */
    void errorOccurred(const QString& errorMessage);

private slots:
    void onReadyRead();
    void onFinished();

private:
    void processSSELine(const QString& line);
    void processSSEEvent(const QString& eventType, const QByteArray& data);

    QNetworkAccessManager* m_nam = nullptr;
    QNetworkReply* m_reply = nullptr;
    QByteArray m_buffer;

    // SSE parser state
    QString m_currentEventType;
    QByteArray m_currentData;

    // Accumulate current content block for contentBlockFinished
    QJsonObject m_currentBlock;
    QString m_accumulatedText;
    QJsonObject m_accumulatedToolInput;
    QString m_toolInputBuffer;  // raw JSON string for tool input

    bool m_busy = false;
};

#endif // LLM_CLIENT_H
