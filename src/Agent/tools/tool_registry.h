#ifndef TOOL_REGISTRY_H
#define TOOL_REGISTRY_H

#include <QObject>
#include <QMap>
#include <QJsonArray>
#include <QJsonObject>

class AgentTool;

class ToolRegistry : public QObject {
    Q_OBJECT
public:
    explicit ToolRegistry(QObject* parent = nullptr);
    ~ToolRegistry() override;

    void registerTool(AgentTool* tool);
    AgentTool* findTool(const QString& name) const;
    QList<AgentTool*> tools() const;
    QJsonArray toolDefinitionsForApi() const;

private:
    QMap<QString, AgentTool*> m_tools;
};

#endif // TOOL_REGISTRY_H
