#include "llm_client.h"
#include "utils/appsettings.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

static const char* kAnthropicUrl = "https://api.anthropic.com/v1/messages";
static const char* kAnthropicVersion = "2023-06-01";

LLMClient::LLMClient(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

LLMClient::~LLMClient()
{
    abort();
}

bool LLMClient::isBusy() const
{
    return m_busy;
}

void LLMClient::abort()
{
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_busy = false;
    m_buffer.clear();
    m_currentEventType.clear();
    m_currentData.clear();
    m_accumulatedText.clear();
    m_toolInputBuffer.clear();
}

void LLMClient::sendMessage(const QJsonArray& messages,
                             const QJsonArray& tools)
{
    if (m_busy) {
        emit errorOccurred(tr("A request is already in progress"));
        return;
    }

    const QString apiKey = AppSettings::llmApiKey();
    if (apiKey.isEmpty()) {
        emit errorOccurred(tr("API key is not configured. Open Settings → AI Agent to set it."));
        return;
    }

    const QString model = AppSettings::llmModel();
    const QString systemPrompt = AppSettings::llmSystemPrompt();

    // Build request body
    QJsonObject body;
    body["model"] = model;
    body["max_tokens"] = AppSettings::llmMaxTokens();
    body["stream"] = true;
    body["messages"] = messages;

    if (!systemPrompt.isEmpty()) {
        body["system"] = systemPrompt;
    }

    if (!tools.isEmpty()) {
        body["tools"] = tools;
    }

    QNetworkRequest request;
    request.setUrl(QUrl(kAnthropicUrl));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("x-api-key", apiKey.toUtf8());
    request.setRawHeader("anthropic-version", kAnthropicVersion);

    m_buffer.clear();
    m_currentEventType.clear();
    m_currentData.clear();
    m_currentBlock = {};
    m_accumulatedText.clear();
    m_accumulatedToolInput = {};
    m_toolInputBuffer.clear();
    m_busy = true;

    m_reply = m_nam->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(m_reply, &QNetworkReply::readyRead, this, &LLMClient::onReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &LLMClient::onFinished);
}

void LLMClient::onReadyRead()
{
    if (!m_reply) return;

    m_buffer.append(m_reply->readAll());

    // SSE lines are delimited by \n
    while (true) {
        int nlPos = m_buffer.indexOf('\n');
        if (nlPos < 0) break;

        QByteArray rawLine = m_buffer.left(nlPos);
        m_buffer.remove(0, nlPos + 1);

        // Remove trailing \r
        if (rawLine.endsWith('\r'))
            rawLine.chop(1);

        QString line = QString::fromUtf8(rawLine);
        processSSELine(line);
    }
}

void LLMClient::processSSELine(const QString& line)
{
    // Empty line = end of event
    if (line.isEmpty()) {
        if (!m_currentData.isEmpty() || !m_currentEventType.isEmpty()) {
            processSSEEvent(m_currentEventType, m_currentData);
            m_currentEventType.clear();
            m_currentData.clear();
        }
        return;
    }

    if (line.startsWith("event: ")) {
        m_currentEventType = line.mid(7).trimmed();
    } else if (line.startsWith("data: ")) {
        if (!m_currentData.isEmpty())
            m_currentData.append('\n');
        m_currentData.append(line.mid(6).toUtf8());
    }
    // Ignore id: and retry: fields
}

void LLMClient::processSSEEvent(const QString& eventType, const QByteArray& data)
{
    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError) {
        // Some events may not be JSON (e.g. "ping")
        return;
    }
    QJsonObject obj = doc.object();

    if (eventType == "message_start") {
        // Nothing special needed — message-level metadata
    }
    else if (eventType == "content_block_start") {
        m_currentBlock = obj.value("content_block").toObject();
        m_accumulatedText.clear();
        m_toolInputBuffer.clear();
    }
    else if (eventType == "content_block_delta") {
        QJsonObject delta = obj.value("delta").toObject();
        QString type = delta.value("type").toString();

        if (type == "text_delta") {
            QString text = delta.value("text").toString();
            m_accumulatedText += text;
            emit streamToken(text);
        }
        else if (type == "input_json_delta") {
            m_toolInputBuffer += delta.value("partial_json").toString();
        }
    }
    else if (eventType == "content_block_stop") {
        // Finalize the block
        QString blockType = m_currentBlock.value("type").toString();

        if (blockType == "text") {
            QJsonObject finished;
            finished["type"] = "text";
            finished["text"] = m_accumulatedText;
            emit contentBlockFinished(finished);
        }
        else if (blockType == "tool_use") {
            QJsonObject finished;
            finished["type"] = "tool_use";
            finished["id"] = m_currentBlock.value("id").toString();
            finished["name"] = m_currentBlock.value("name").toString();

            // Parse accumulated tool input JSON
            QJsonDocument inputDoc = QJsonDocument::fromJson(m_toolInputBuffer.toUtf8());
            finished["input"] = inputDoc.isObject() ? inputDoc.object() : QJsonObject();

            emit contentBlockFinished(finished);
        }

        m_currentBlock = {};
        m_accumulatedText.clear();
        m_toolInputBuffer.clear();
    }
    else if (eventType == "message_delta") {
        // Contains stop_reason in delta
        QJsonObject delta = obj.value("delta").toObject();
        QString stopReason = delta.value("stop_reason").toString();
        if (!stopReason.isEmpty()) {
            m_busy = false;
            emit streamFinished(stopReason);
        }
    }
    else if (eventType == "message_stop") {
        m_busy = false;
        // streamFinished was already emitted from message_delta
    }
    else if (eventType == "error") {
        QJsonObject error = obj.value("error").toObject();
        QString msg = error.value("message").toString();
        if (msg.isEmpty()) msg = QString::fromUtf8(data);
        m_busy = false;
        emit errorOccurred(msg);
    }
    else if (eventType == "ping") {
        // Keep-alive, ignore
    }
}

void LLMClient::onFinished()
{
    if (!m_reply) return;

    if (m_reply->error() != QNetworkReply::NoError &&
        m_reply->error() != QNetworkReply::OperationCanceledError) {

        // Try to parse error body
        QByteArray body = m_reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(body);
        QString errMsg;

        if (doc.isObject()) {
            QJsonObject errObj = doc.object().value("error").toObject();
            errMsg = errObj.value("message").toString();
        }

        if (errMsg.isEmpty()) {
            errMsg = m_reply->errorString();
        }

        m_busy = false;
        emit errorOccurred(errMsg);
    }

    m_reply->deleteLater();
    m_reply = nullptr;
    m_busy = false;
}
