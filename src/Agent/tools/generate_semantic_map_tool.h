#ifndef GENERATE_SEMANTIC_MAP_TOOL_H
#define GENERATE_SEMANTIC_MAP_TOOL_H

#include "agent_tool.h"
#include "../ToolTabs/Canvas/semantic_map.h"
#include <QStringList>

class LLMClient;
class EndpointManager;

struct ValidationResult {
    bool ok = false;
    SemanticMap map;
    QString errorReason;
};

class GenerateSemanticMapTool : public AgentTool
{
    Q_OBJECT
public:
    explicit GenerateSemanticMapTool(const QString& projectRoot,
                                     EndpointManager* endpointManager,
                                     QObject* parent = nullptr);

    QString name() const override { return "generate_semantic_map"; }
    QString description() const override;
    QJsonObject parameters() const override;
    void execute(const QJsonObject& args) override;

private slots:
    void onMapStreamToken(const QString& token);
    void onMapStreamFinished(const QString& stopReason);
    void onMapErrorOccurred(const QString& errorMessage);

private:
    QString m_projectRoot;
    EndpointManager* m_endpointManager;
    LLMClient* m_innerClient = nullptr;
    QString m_streamBuffer;
    int m_retryCount = 0;
    QStringList m_availableFiles;
    QString m_originalSystemPrompt;
    QString m_originalUserPrompt;
    static constexpr int kMaxRetries = 2;
    static constexpr int kMaxContextChars = 60000;

    QString buildSystemPrompt() const;
    QString buildUserPrompt(const QJsonObject& args) const;
    void sendGenerationRequest(const QString& systemPrompt, const QString& userPrompt);
    QString gatherFileContents(const QStringList& scope) const;
    ValidationResult validateSemanticMapJson(const QString& rawJson) const;
    int countFileLines(const QString& fullPath) const;
};

#endif // GENERATE_SEMANTIC_MAP_TOOL_H
