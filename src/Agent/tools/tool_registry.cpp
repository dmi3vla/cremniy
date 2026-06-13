#include "tool_registry.h"
#include "agent_tool.h"

ToolRegistry::ToolRegistry(QObject* parent)
    : QObject(parent)
{
}

ToolRegistry::~ToolRegistry()
{
    qDeleteAll(m_tools);
}

void ToolRegistry::registerTool(AgentTool* tool)
{
    if (tool) {
        tool->setParent(this);
        m_tools[tool->name()] = tool;
    }
}

AgentTool* ToolRegistry::findTool(const QString& name) const
{
    return m_tools.value(name, nullptr);
}

QList<AgentTool*> ToolRegistry::tools() const
{
    return m_tools.values();
}

QJsonArray ToolRegistry::toolDefinitionsForApi() const
{
    QJsonArray definitions;
    for (AgentTool* tool : m_tools) {
        QJsonObject def;
        def["name"] = tool->name();
        def["description"] = tool->description();
        def["input_schema"] = tool->parameters();
        definitions.append(def);
    }
    return definitions;
}
