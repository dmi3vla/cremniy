#include "agent_session.h"
#include "llm_client.h"
#include "conversation_history.h"
#include "chat_panel.h"
#include "chat_message.h"
#include "tools/agent_tool.h"
#include "tools/tool_registry.h"
#include "tools/agent_tools.h"
#include "tools/generate_codemap_tool.h"
#include "endpoint_manager.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QVariant>
#include <QApplication>

#include "app/IDEWindow/idewindow.h"
#include "widgets/filetab.h"
#include "ui/filestabwidget.h"
#include "ui/toolstabwidget.h"
#include "ToolTabs/CodeEditor/codeeditortab.h"
#include "ToolTabs/Canvas/canvastab.h"
#include "ToolTabs/Canvas/canvas_view.h"
#include "ToolTabs/Canvas/nodes/file_node.h"

AgentSession::AgentSession(const QString& projectRoot, ChatPanel* chatPanel, QObject* parent)
    : QObject(parent)
    , m_projectRoot(projectRoot)
    , m_chatPanel(chatPanel)
    , m_client(new LLMClient(this))
    , m_history(new ConversationHistory(this))
    , m_tools(new ToolRegistry(this))
{
    // Register tools
    m_tools->registerTool(new ReadFileTool(m_projectRoot, this));
    m_tools->registerTool(new WriteFileTool(m_projectRoot, this));
    m_tools->registerTool(new ListDirTool(m_projectRoot, this));
    m_tools->registerTool(new SearchGrepTool(m_projectRoot, this));
    m_tools->registerTool(new GetDependenciesTool(this));
    m_tools->registerTool(new OpenFileTool(this));
    m_tools->registerTool(new HighlightDependenciesTool(this));
    m_tools->registerTool(new GenerateCodemapTool(m_projectRoot, &EndpointManager::instance(), this));

    // Initialize conversation history
    m_history->setProjectRoot(m_projectRoot);

    // Connections
    connect(m_chatPanel, &ChatPanel::messageSent, this, &AgentSession::onMessageSent);
    
    connect(m_client, &LLMClient::streamToken, this, &AgentSession::onStreamToken);
    connect(m_client, &LLMClient::contentBlockFinished, this, &AgentSession::onContentBlockFinished);
    connect(m_client, &LLMClient::streamFinished, this, &AgentSession::onStreamFinished);
    connect(m_client, &LLMClient::errorOccurred, this, &AgentSession::onErrorOccurred);

    // Load existing history and display in chat
    if (m_history->load()) {
        const QJsonArray msgs = m_history->messages();
        for (const QJsonValue& val : msgs) {
            QJsonObject obj = val.toObject();
            QString role = obj["role"].toString();
            QJsonValue content = obj["content"];
            
            if (role == "user") {
                if (content.isString()) {
                    m_chatPanel->addMessage(content.toString(), ChatMessage::User);
                } else if (content.isArray()) {
                    for (const QJsonValue& cVal : content.toArray()) {
                        QJsonObject cObj = cVal.toObject();
                        if (cObj["type"].toString() == "tool_result") {
                            QString toolId = cObj["tool_use_id"].toString();
                            QString res = cObj["content"].toString();
                            bool isErr = cObj["is_error"].toBool();
                            m_chatPanel->addMessage(tr("Tool Result [%1]: %2").arg(toolId, isErr ? tr("Error") : res), ChatMessage::ToolResult);
                            ChatMessage* last = m_chatPanel->lastMessage();
                            if (last) last->setExpanded(false);
                        }
                    }
                }
            } else if (role == "assistant") {
                if (content.isString()) {
                    m_chatPanel->addMessage(content.toString(), ChatMessage::Assistant);
                } else if (content.isArray()) {
                    for (const QJsonValue& cVal : content.toArray()) {
                        QJsonObject cObj = cVal.toObject();
                        QString type = cObj["type"].toString();
                        if (type == "text") {
                            m_chatPanel->addMessage(cObj["text"].toString(), ChatMessage::Assistant);
                        } else if (type == "tool_use") {
                            QString toolName = cObj["name"].toString();
                            QJsonObject input = cObj["input"].toObject();
                            QJsonDocument doc(input);
                            m_chatPanel->addMessage(tr("Tool Call: %1\nArguments:\n%2").arg(toolName, QString::fromUtf8(doc.toJson(QJsonDocument::Indented))), ChatMessage::ToolCall);
                            ChatMessage* last = m_chatPanel->lastMessage();
                            if (last) last->setExpanded(false);
                        }
                    }
                }
            }
        }
    }
}

AgentSession::~AgentSession()
{
}

bool AgentSession::isBusy() const
{
    return m_busy;
}

void AgentSession::cancel()
{
    m_client->abort();
    m_busy = false;
    m_waitingForTools = false;
    m_pendingToolCallsCount = 0;
    m_currentAssistantBlocks = QJsonArray();
    m_chatPanel->addMessage(tr("Agent session cancelled by user."), ChatMessage::System);
}

void AgentSession::onMessageSent(const QString& text)
{
    if (m_busy) return;

    m_busy = true;
    m_iterationCount = 0;

    // Build context info
    QString contextStr;
    IDEWindow* ideWin = nullptr;
    for (QWidget* w : QApplication::topLevelWidgets()) {
        ideWin = qobject_cast<IDEWindow*>(w);
        if (ideWin) break;
    }
    if (ideWin) {
        auto* filesTabWidget = ideWin->findChild<FilesTabWidget*>();
        if (filesTabWidget) {
            auto* currentTab = qobject_cast<FileTab*>(filesTabWidget->currentWidget());
            if (currentTab) {
                contextStr += QString("\n[Context - Active File: %1]").arg(currentTab->filePath);
                
                auto* toolsTab = currentTab->findChild<ToolsTabWidget*>();
                if (toolsTab) {
                    auto* codeTab = toolsTab->findChild<CodeEditorTab*>();
                    if (codeTab) {
                        auto* codeEditor = codeTab->findChild<QCodeEditor*>();
                        if (codeEditor) {
                            QString selectedText = codeEditor->textCursor().selectedText();
                            if (!selectedText.isEmpty()) {
                                contextStr += QString("\n[Context - Selected text in editor:\n%1\n]").arg(selectedText);
                            }
                        }
                    }
                    
                    auto* canvasTab = toolsTab->findChild<CanvasTab*>();
                    if (canvasTab) {
                        auto* canvasView = canvasTab->findChild<CanvasView*>();
                        if (canvasView && canvasView->scene()) {
                            QStringList selectedFiles;
                            for (QGraphicsItem* item : canvasView->scene()->selectedItems()) {
                                auto* node = qgraphicsitem_cast<FileNode*>(item);
                                if (node) {
                                    selectedFiles.append(node->filePath());
                                }
                            }
                            if (!selectedFiles.isEmpty()) {
                                contextStr += QString("\n[Context - Selected nodes on canvas: %1]").arg(selectedFiles.join(", "));
                            }
                        }
                    }
                }
            }
        }
    }

    m_history->addUserMessage(text + contextStr);
    
    // Add assistant bubble for streaming response
    m_chatPanel->addMessage("", ChatMessage::Assistant);

    m_client->sendMessage(m_history->messages(), m_tools->toolDefinitionsForApi());
}

void AgentSession::onStreamToken(const QString& token)
{
    m_chatPanel->appendToLastMessage(token);
}

void AgentSession::onContentBlockFinished(const QJsonObject& block)
{
    m_currentAssistantBlocks.append(block);
    
    if (block["type"].toString() == "tool_use") {
        executeToolUse(block);
    }
}

void AgentSession::onStreamFinished(const QString& stopReason)
{
    m_history->addAssistantBlocks(m_currentAssistantBlocks);
    m_currentAssistantBlocks = QJsonArray();

    if (stopReason == "tool_use") {
        m_waitingForTools = true;
        if (m_pendingToolCallsCount == 0) {
            m_waitingForTools = false;
            triggerNextTurn();
        }
    } else {
        m_busy = false;
        m_waitingForTools = false;
        m_pendingToolCallsCount = 0;
    }
}

void AgentSession::onErrorOccurred(const QString& errorMessage)
{
    m_chatPanel->addMessage(tr("Error: %1").arg(errorMessage), ChatMessage::System);
    m_busy = false;
    m_waitingForTools = false;
    m_pendingToolCallsCount = 0;
}

void AgentSession::executeToolUse(const QJsonObject& toolUseObj)
{
    QString id = toolUseObj["id"].toString();
    QString name = toolUseObj["name"].toString();
    QJsonObject input = toolUseObj["input"].toObject();

    m_pendingToolCallsCount++;

    QJsonDocument inputDoc(input);
    QString argsStr = QString::fromUtf8(inputDoc.toJson(QJsonDocument::Indented));
    m_chatPanel->addMessage(tr("Calling Tool: %1\nArguments:\n%2").arg(name, argsStr), ChatMessage::ToolCall);
    ChatMessage* msgBubble = m_chatPanel->lastMessage();
    if (msgBubble) {
        msgBubble->setExpanded(false);
    }

    // Try starting node pulsing if path/file is in arguments
    QString path = input["path"].toString();
    if (path.isEmpty()) {
        path = input["file"].toString();
    }
    if (!path.isEmpty()) {
        QString resolved;
        if (validateAndResolvePath(m_projectRoot, path, resolved)) {
            IDEWindow* ideWin = nullptr;
            for (QWidget* w : QApplication::topLevelWidgets()) {
                ideWin = qobject_cast<IDEWindow*>(w);
                if (ideWin) break;
            }
            if (ideWin) {
                auto* filesTabWidget = ideWin->findChild<FilesTabWidget*>();
                if (filesTabWidget) {
                    auto* currentTab = qobject_cast<FileTab*>(filesTabWidget->currentWidget());
                    if (currentTab) {
                        auto* toolsTab = currentTab->findChild<ToolsTabWidget*>();
                        if (toolsTab) {
                            auto* canvasTab = toolsTab->findChild<CanvasTab*>();
                            if (canvasTab) {
                                canvasTab->startNodePulsing(resolved);
                            }
                        }
                    }
                }
            }
        }
    }

    AgentTool* tool = m_tools->findTool(name);
    if (!tool) {
        onToolFinished(nullptr, id, tr("Error: Tool '%1' not found").arg(name), true);
        return;
    }

    connect(tool, &AgentTool::finished, this, [this, tool, id](const QString& result, bool isError) {
        tool->disconnect(this);
        onToolFinished(tool, id, result, isError);
    });

    tool->execute(input);
}

void AgentSession::onToolFinished(AgentTool* tool, const QString& toolUseId, const QString& result, bool isError)
{
    Q_UNUSED(tool);
    m_pendingToolCallsCount--;

    QString summary = isError ? tr("Failed: %1").arg(result) : tr("Success: %1").arg(result);
    m_chatPanel->addMessage(tr("Tool Result [%1]:\n%2").arg(toolUseId, summary), ChatMessage::ToolResult);
    ChatMessage* msgBubble = m_chatPanel->lastMessage();
    if (msgBubble) {
        msgBubble->setExpanded(false);
    }

    // Stop node pulsing
    IDEWindow* ideWin = nullptr;
    for (QWidget* w : QApplication::topLevelWidgets()) {
        ideWin = qobject_cast<IDEWindow*>(w);
        if (ideWin) break;
    }
    if (ideWin) {
        auto* filesTabWidget = ideWin->findChild<FilesTabWidget*>();
        if (filesTabWidget) {
            auto* currentTab = qobject_cast<FileTab*>(filesTabWidget->currentWidget());
            if (currentTab) {
                auto* toolsTab = currentTab->findChild<ToolsTabWidget*>();
                if (toolsTab) {
                    auto* canvasTab = toolsTab->findChild<CanvasTab*>();
                    if (canvasTab) {
                        canvasTab->stopNodePulsing();
                    }
                }
            }
        }
    }

    m_history->addToolResult(toolUseId, result, isError);

    if (m_pendingToolCallsCount == 0 && m_waitingForTools) {
        m_waitingForTools = false;
        triggerNextTurn();
    }
}

void AgentSession::triggerNextTurn()
{
    m_iterationCount++;
    if (m_iterationCount >= m_maxIterations) {
        m_chatPanel->addMessage(tr("System: Iteration limit reached (%1 iterations). Stopped to prevent infinite loop.").arg(m_maxIterations), ChatMessage::System);
        m_busy = false;
        return;
    }

    m_chatPanel->addMessage("", ChatMessage::Assistant);
    m_client->sendMessage(m_history->messages(), m_tools->toolDefinitionsForApi());
}
