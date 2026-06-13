#include "llm_client.h"

#include "Agent/endpoint_manager.h"
#include "utils/appsettings.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

static const char* kAnthropicVersion = "2023-06-01";

LLMClient::LLMClient(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    connect(&EndpointManager::instance(), &EndpointManager::activeEndpointChanged, this, [this]() {
        if (m_busy) {
            abort();
            emit errorOccurred(tr("Active endpoint changed. Current request was aborted."));
        }
    });
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
    m_accumulatedOpenAIToolCalls.clear();
}

QUrl LLMClient::endpointUrl(const ApiEndpoint& endpoint) const
{
    QString base = endpoint.baseUrl.trimmed();
    while (base.endsWith('/')) {
        base.chop(1);
    }

    if (endpoint.format == ApiEndpoint::ApiFormat::OpenAIChatCompletions) {
        return QUrl(base + QStringLiteral("/chat/completions"));
    }
    return QUrl(base + QStringLiteral("/messages"));
}

void LLMClient::sendMessage(const QJsonArray& messages,
                             const QJsonArray& tools)
{
    if (m_busy) {
        emit errorOccurred(tr("A request is already in progress"));
        return;
    }

    const ApiEndpoint endpoint = EndpointManager::instance().activeEndpoint();
    if (endpoint.baseUrl.trimmed().isEmpty()) {
        emit errorOccurred(tr("Active endpoint base URL is not configured. Open Settings → AI Assistant."));
        return;
    }
    if (endpoint.defaultModel.trimmed().isEmpty()) {
        emit errorOccurred(tr("Model is not configured for active endpoint '%1'. Open Settings → AI Assistant.").arg(endpoint.name));
        return;
    }
    if (endpoint.format == ApiEndpoint::ApiFormat::AnthropicMessages && endpoint.apiKey.trimmed().isEmpty()) {
        emit errorOccurred(tr("API key is not configured for active endpoint '%1'. Open Settings → AI Assistant.").arg(endpoint.name));
        return;
    }

    QJsonObject body = endpoint.format == ApiEndpoint::ApiFormat::OpenAIChatCompletions
        ? buildOpenAIRequest(messages, tools)
        : buildAnthropicRequest(messages, tools);

    QNetworkRequest request;
    request.setUrl(endpointUrl(endpoint));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (endpoint.format == ApiEndpoint::ApiFormat::OpenAIChatCompletions) {
        if (!endpoint.apiKey.trimmed().isEmpty()) {
            request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(endpoint.apiKey.trimmed()).toUtf8());
        }
    } else {
        request.setRawHeader("x-api-key", endpoint.apiKey.trimmed().toUtf8());
        request.setRawHeader("anthropic-version", kAnthropicVersion);
    }

    m_buffer.clear();
    m_currentEventType.clear();
    m_currentData.clear();
    m_currentBlock = {};
    m_accumulatedText.clear();
    m_accumulatedToolInput = {};
    m_toolInputBuffer.clear();
    m_accumulatedOpenAIToolCalls.clear();
    m_currentRequestIsOpenAI = endpoint.format == ApiEndpoint::ApiFormat::OpenAIChatCompletions;
    m_busy = true;

    m_reply = m_nam->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_reply, &QNetworkReply::readyRead, this, &LLMClient::onReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &LLMClient::onFinished);
}

QJsonObject LLMClient::buildAnthropicRequest(const QJsonArray& messages, const QJsonArray& tools)
{
    const ApiEndpoint endpoint = EndpointManager::instance().activeEndpoint();

    QJsonObject body;
    body["model"] = endpoint.defaultModel.trimmed();
    body["max_tokens"] = AppSettings::llmMaxTokens();
    body["stream"] = true;
    body["messages"] = messages;

    const QString systemPrompt = AppSettings::llmSystemPrompt();
    if (!systemPrompt.isEmpty()) {
        body["system"] = systemPrompt;
    }
    if (!tools.isEmpty()) {
        body["tools"] = tools;
    }
    return body;
}

QJsonObject LLMClient::buildOpenAIRequest(const QJsonArray& messages, const QJsonArray& tools)
{
    const ApiEndpoint endpoint = EndpointManager::instance().activeEndpoint();

    QJsonObject body;
    body["model"] = endpoint.defaultModel.trimmed();
    body["max_tokens"] = AppSettings::llmMaxTokens();
    body["stream"] = true;
    body["messages"] = convertMessagesForOpenAI(messages);

    const QJsonArray convertedTools = convertToolsForOpenAI(tools);
    if (!convertedTools.isEmpty()) {
        body["tools"] = convertedTools;
        body["tool_choice"] = QStringLiteral("auto");
    }
    return body;
}

QJsonArray LLMClient::convertMessagesForOpenAI(const QJsonArray& messages) const
{
    QJsonArray out;
    const QString systemPrompt = AppSettings::llmSystemPrompt();
    if (!systemPrompt.isEmpty()) {
        QJsonObject sys;
        sys["role"] = "system";
        sys["content"] = systemPrompt;
        out.append(sys);
    }

    for (const QJsonValue& value : messages) {
        const QJsonObject msg = value.toObject();
        QJsonObject converted;
        const QString role = msg.value("role").toString();
        converted["role"] = role;

        const QJsonValue content = msg.value("content");
        if (content.isString()) {
            converted["content"] = content.toString();
            out.append(converted);
            continue;
        }

        if (!content.isArray()) {
            converted["content"] = QString();
            out.append(converted);
            continue;
        }

        QJsonArray textParts;
        QJsonArray toolCalls;
        bool hasToolResult = false;
        for (const QJsonValue& blockValue : content.toArray()) {
            const QJsonObject block = blockValue.toObject();
            const QString type = block.value("type").toString();
            if (type == QLatin1String("text")) {
                textParts.append(block.value("text").toString());
            } else if (type == QLatin1String("tool_use")) {
                QJsonObject function;
                function["name"] = block.value("name").toString();
                function["arguments"] = QString::fromUtf8(QJsonDocument(block.value("input").toObject()).toJson(QJsonDocument::Compact));

                QJsonObject call;
                call["id"] = block.value("id").toString();
                call["type"] = "function";
                call["function"] = function;
                toolCalls.append(call);
            } else if (type == QLatin1String("tool_result")) {
                QJsonObject toolMsg;
                toolMsg["role"] = "tool";
                toolMsg["tool_call_id"] = block.value("tool_use_id").toString();
                toolMsg["content"] = block.value("content").toString();
                out.append(toolMsg);
                hasToolResult = true;
            }
        }

        if (hasToolResult && textParts.isEmpty() && toolCalls.isEmpty()) {
            continue;
        }

        QStringList textList;
        for (const QJsonValue& text : textParts) {
            textList << text.toString();
        }
        converted["content"] = textList.join('\n');
        if (!toolCalls.isEmpty()) {
            converted["tool_calls"] = toolCalls;
        }
        out.append(converted);
    }
    return out;
}

QJsonArray LLMClient::convertToolsForOpenAI(const QJsonArray& tools) const
{
    QJsonArray out;
    for (const QJsonValue& value : tools) {
        const QJsonObject tool = value.toObject();
        QJsonObject function;
        function["name"] = tool.value("name").toString();
        function["description"] = tool.value("description").toString();
        function["parameters"] = tool.value("input_schema").toObject();

        QJsonObject wrapper;
        wrapper["type"] = "function";
        wrapper["function"] = function;
        out.append(wrapper);
    }
    return out;
}

void LLMClient::onReadyRead()
{
    if (!m_reply) return;

    m_buffer.append(m_reply->readAll());
    while (true) {
        int nlPos = m_buffer.indexOf('\n');
        if (nlPos < 0) break;

        QByteArray rawLine = m_buffer.left(nlPos);
        m_buffer.remove(0, nlPos + 1);
        if (rawLine.endsWith('\r'))
            rawLine.chop(1);

        processSSELine(QString::fromUtf8(rawLine));
    }
}

void LLMClient::processSSELine(const QString& line)
{
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
}

void LLMClient::processSSEEvent(const QString& eventType, const QByteArray& data)
{
    if (m_currentRequestIsOpenAI) {
        processOpenAISSEEvent(data);
    } else {
        processAnthropicSSEEvent(eventType, data);
    }
}

void LLMClient::processAnthropicSSEEvent(const QString& eventType, const QByteArray& data)
{
    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError) {
        return;
    }
    QJsonObject obj = doc.object();

    if (eventType == "content_block_start") {
        m_currentBlock = obj.value("content_block").toObject();
        m_accumulatedText.clear();
        m_toolInputBuffer.clear();
    } else if (eventType == "content_block_delta") {
        QJsonObject delta = obj.value("delta").toObject();
        QString type = delta.value("type").toString();

        if (type == "text_delta") {
            QString text = delta.value("text").toString();
            m_accumulatedText += text;
            emit streamToken(text);
        } else if (type == "input_json_delta") {
            m_toolInputBuffer += delta.value("partial_json").toString();
        }
    } else if (eventType == "content_block_stop") {
        const QString blockType = m_currentBlock.value("type").toString();
        if (blockType == "text") {
            QJsonObject finished;
            finished["type"] = "text";
            finished["text"] = m_accumulatedText;
            emit contentBlockFinished(finished);
        } else if (blockType == "tool_use") {
            QJsonObject finished;
            finished["type"] = "tool_use";
            finished["id"] = m_currentBlock.value("id").toString();
            finished["name"] = m_currentBlock.value("name").toString();
            const QJsonDocument inputDoc = QJsonDocument::fromJson(m_toolInputBuffer.toUtf8());
            finished["input"] = inputDoc.isObject() ? inputDoc.object() : QJsonObject();
            emit contentBlockFinished(finished);
        }
        m_currentBlock = {};
        m_accumulatedText.clear();
        m_toolInputBuffer.clear();
    } else if (eventType == "message_delta") {
        QJsonObject delta = obj.value("delta").toObject();
        QString stopReason = delta.value("stop_reason").toString();
        if (!stopReason.isEmpty()) {
            m_busy = false;
            emit streamFinished(stopReason);
        }
    } else if (eventType == "message_stop") {
        m_busy = false;
    } else if (eventType == "error") {
        QJsonObject error = obj.value("error").toObject();
        QString msg = error.value("message").toString();
        if (msg.isEmpty()) msg = QString::fromUtf8(data);
        m_busy = false;
        emit errorOccurred(msg);
    }
}

void LLMClient::processOpenAISSEEvent(const QByteArray& data)
{
    if (!m_busy) {
        return;
    }

    if (data.trimmed() == "[DONE]") {
        if (!m_accumulatedText.isEmpty()) {
            QJsonObject finished;
            finished["type"] = "text";
            finished["text"] = m_accumulatedText;
            emit contentBlockFinished(finished);
            m_accumulatedText.clear();
        }
        for (auto it = m_accumulatedOpenAIToolCalls.cbegin(); it != m_accumulatedOpenAIToolCalls.cend(); ++it) {
            const QJsonObject call = it.value();
            const QJsonObject function = call.value("function").toObject();
            QJsonObject finished;
            finished["type"] = "tool_use";
            finished["id"] = call.value("id").toString();
            finished["name"] = function.value("name").toString();
            const QJsonDocument argsDoc = QJsonDocument::fromJson(function.value("arguments").toString().toUtf8());
            finished["input"] = argsDoc.isObject() ? argsDoc.object() : QJsonObject();
            emit contentBlockFinished(finished);
        }
        const bool hadTools = !m_accumulatedOpenAIToolCalls.isEmpty();
        m_accumulatedOpenAIToolCalls.clear();
        m_busy = false;
        emit streamFinished(hadTools ? QStringLiteral("tool_use") : QStringLiteral("stop"));
        return;
    }

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        return;
    }

    const QJsonArray choices = doc.object().value("choices").toArray();
    if (choices.isEmpty()) {
        return;
    }

    const QJsonObject choice = choices.first().toObject();
    const QJsonObject delta = choice.value("delta").toObject();
    const QString text = delta.value("content").toString();
    if (!text.isEmpty()) {
        m_accumulatedText += text;
        emit streamToken(text);
    }

    const QJsonArray toolCalls = delta.value("tool_calls").toArray();
    for (const QJsonValue& callValue : toolCalls) {
        const QJsonObject deltaCall = callValue.toObject();
        const int index = deltaCall.value("index").toInt();
        QJsonObject current = m_accumulatedOpenAIToolCalls.value(index);
        if (deltaCall.contains("id")) {
            current["id"] = deltaCall.value("id").toString();
        }
        current["type"] = "function";

        QJsonObject currentFunction = current.value("function").toObject();
        const QJsonObject deltaFunction = deltaCall.value("function").toObject();
        if (deltaFunction.contains("name")) {
            currentFunction["name"] = currentFunction.value("name").toString() + deltaFunction.value("name").toString();
        }
        if (deltaFunction.contains("arguments")) {
            currentFunction["arguments"] = currentFunction.value("arguments").toString() + deltaFunction.value("arguments").toString();
        }
        current["function"] = currentFunction;
        m_accumulatedOpenAIToolCalls[index] = current;
    }

    const QString finishReason = choice.value("finish_reason").toString();
    if (finishReason == "stop" && m_accumulatedOpenAIToolCalls.isEmpty()) {
        if (!m_accumulatedText.isEmpty()) {
            QJsonObject finished;
            finished["type"] = "text";
            finished["text"] = m_accumulatedText;
            emit contentBlockFinished(finished);
            m_accumulatedText.clear();
        }
        m_busy = false;
        emit streamFinished("stop");
    }
}

void LLMClient::onFinished()
{
    if (!m_reply) return;

    if (m_reply->error() != QNetworkReply::NoError &&
        m_reply->error() != QNetworkReply::OperationCanceledError) {
        QByteArray body = m_reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(body);
        QString errMsg;

        if (doc.isObject()) {
            const QJsonValue errorValue = doc.object().value("error");
            if (errorValue.isObject()) {
                errMsg = errorValue.toObject().value("message").toString();
            } else if (errorValue.isString()) {
                errMsg = errorValue.toString();
            }
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
