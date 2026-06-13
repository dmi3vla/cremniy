#ifndef AGENT_TOOL_H
#define AGENT_TOOL_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>

class AgentTool : public QObject {
    Q_OBJECT
public:
    explicit AgentTool(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~AgentTool() = default;

    virtual QString name() const = 0;
    virtual QString description() const = 0;
    virtual QJsonObject parameters() const = 0;

    /**
     * @brief Execute the tool.
     * When finished, emits the finished signal.
     */
    virtual void execute(const QJsonObject& args) = 0;

signals:
    void finished(const QString& result, bool isError);
};

#endif // AGENT_TOOL_H
