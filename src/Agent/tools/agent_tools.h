#ifndef AGENT_TOOLS_H
#define AGENT_TOOLS_H

#include "agent_tool.h"
#include <QDialog>

class QTextBrowser;

// --- Helper path validation ---
bool validateAndResolvePath(const QString& projectRoot, const QString& inputPath, QString& resolvedPath);

// --- Write Approval Dialog ---
class DiffDialog : public QDialog {
    Q_OBJECT
public:
    enum Result { Rejected, Approved, AlwaysAllow };

    DiffDialog(const QString& filePath, const QString& oldContent, const QString& newContent, QWidget* parent = nullptr);
    Result resultType() const { return m_result; }

private:
    Result m_result = Rejected;
    QTextBrowser* m_diffView = nullptr;
};

// --- 1. ReadFileTool ---
class ReadFileTool : public AgentTool {
    Q_OBJECT
public:
    explicit ReadFileTool(const QString& projectRoot, QObject* parent = nullptr);
    QString name() const override { return "read_file"; }
    QString description() const override;
    QJsonObject parameters() const override;
    void execute(const QJsonObject& args) override;

private:
    QString m_projectRoot;
};

// --- 2. WriteFileTool ---
class WriteFileTool : public AgentTool {
    Q_OBJECT
public:
    explicit WriteFileTool(const QString& projectRoot, QObject* parent = nullptr);
    QString name() const override { return "write_file"; }
    QString description() const override;
    QJsonObject parameters() const override;
    void execute(const QJsonObject& args) override;

    void setAlwaysAllow(bool allow) { m_alwaysAllow = allow; }
    bool alwaysAllow() const { return m_alwaysAllow; }

private:
    QString m_projectRoot;
    bool m_alwaysAllow = false;
};

// --- 3. ListDirTool ---
class ListDirTool : public AgentTool {
    Q_OBJECT
public:
    explicit ListDirTool(const QString& projectRoot, QObject* parent = nullptr);
    QString name() const override { return "list_directory"; }
    QString description() const override;
    QJsonObject parameters() const override;
    void execute(const QJsonObject& args) override;

private:
    QString m_projectRoot;
};

// --- 4. SearchGrepTool ---
class SearchGrepTool : public AgentTool {
    Q_OBJECT
public:
    explicit SearchGrepTool(const QString& projectRoot, QObject* parent = nullptr);
    QString name() const override { return "search_grep"; }
    QString description() const override;
    QJsonObject parameters() const override;
    void execute(const QJsonObject& args) override;

private:
    QString m_projectRoot;
};

// --- 5. GetDependenciesTool ---
class GetDependenciesTool : public AgentTool {
    Q_OBJECT
public:
    explicit GetDependenciesTool(QObject* parent = nullptr);
    QString name() const override { return "get_dependencies"; }
    QString description() const override;
    QJsonObject parameters() const override;
    void execute(const QJsonObject& args) override;
};

// --- 6. OpenFileTool ---
class OpenFileTool : public AgentTool {
    Q_OBJECT
public:
    explicit OpenFileTool(QObject* parent = nullptr);
    QString name() const override { return "open_file"; }
    QString description() const override;
    QJsonObject parameters() const override;
    void execute(const QJsonObject& args) override;
};

// --- 7. HighlightDependenciesTool ---
class HighlightDependenciesTool : public AgentTool {
    Q_OBJECT
public:
    explicit HighlightDependenciesTool(QObject* parent = nullptr);
    QString name() const override { return "highlight_dependencies"; }
    QString description() const override;
    QJsonObject parameters() const override;
    void execute(const QJsonObject& args) override;
};

#endif // AGENT_TOOLS_H
