#ifndef GENERATE_CODEMAP_TOOL_H
#define GENERATE_CODEMAP_TOOL_H

#include "agent_tool.h"
#include "../ToolTabs/Canvas/codemap.h"
#include <QStringList>

class LLMClient;
class EndpointManager;

struct CodemapValidationResult {
    bool ok = false;
    Codemap map;
    QString errorReason;
};

class GenerateCodemapTool : public AgentTool
{
    Q_OBJECT
#ifdef CREMNIY_TESTING
    friend class TestGenerateCodemapTool;
#endif
public:
    explicit GenerateCodemapTool(const QString& projectRoot,
                                  EndpointManager* endpointManager,
                                  QObject* parent = nullptr);

    QString name() const override { return "generate_codemap"; }
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
    CodemapValidationResult validateCodemapJson(const QString& rawJson) const;
    int countFileLines(const QString& fullPath) const;
    QString readLineContent(const QString& fullPath, int lineNumber) const;
};

#endif // GENERATE_CODEMAP_TOOL_H
