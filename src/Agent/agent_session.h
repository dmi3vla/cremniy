#ifndef AGENT_SESSION_H
#define AGENT_SESSION_H

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>

class LLMClient;
class ConversationHistory;
class ToolRegistry;
class ChatPanel;
class AgentTool;

class AgentSession : public QObject
{
    Q_OBJECT
public:
    explicit AgentSession(const QString& projectRoot, ChatPanel* chatPanel, QObject* parent = nullptr);
    ~AgentSession() override;

    bool isBusy() const;
    void cancel();

private slots:
    void onMessageSent(const QString& text);
    void onStreamToken(const QString& token);
    void onContentBlockFinished(const QJsonObject& block);
    void onStreamFinished(const QString& stopReason);
    void onErrorOccurred(const QString& errorMessage);

    void onToolFinished(AgentTool* tool, const QString& toolUseId, const QString& result, bool isError);

private:
    void executeToolUse(const QJsonObject& toolUseObj);
    void triggerNextTurn();

    QString m_projectRoot;
    ChatPanel* m_chatPanel = nullptr;
    LLMClient* m_client = nullptr;
    ConversationHistory* m_history = nullptr;
    ToolRegistry* m_tools = nullptr;

    // Session state
    bool m_busy = false;
    bool m_waitingForTools = false;
    int m_pendingToolCallsCount = 0;
    int m_iterationCount = 0;
    const int m_maxIterations = 10;

    // Accumulates blocks of the current assistant turn
    QJsonArray m_currentAssistantBlocks;
};

#endif // AGENT_SESSION_H
