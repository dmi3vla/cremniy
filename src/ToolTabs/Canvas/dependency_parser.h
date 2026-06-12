#ifndef DEPENDENCY_PARSER_H
#define DEPENDENCY_PARSER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QFileSystemWatcher>
#include <unordered_map>

struct DependencyGraph {
    std::unordered_map<QString, QStringList> includes;
    QStringList allFiles;

    void clear() {
        includes.clear();
        allFiles.clear();
    }
};

class DependencyParserWorker : public QObject
{
    Q_OBJECT

public:
    explicit DependencyParserWorker(const QString& projectPath, QObject* parent = nullptr);

public slots:
    void parse();

signals:
    void graphReady(DependencyGraph graph);
    void progress(int current, int total);

private:
    QString m_projectPath;

    QStringList findSourceFiles(const QString& dir) const;
    QStringList parseIncludes(const QString& filePath) const;
    QString resolveInclude(const QString& includePath, const QString& fromFile) const;
};

class DependencyParser : public QObject
{
    Q_OBJECT

public:
    explicit DependencyParser(const QString& projectPath, QObject* parent = nullptr);

    void startParsing();
    void watchForChanges();

    DependencyGraph graph() const { return m_graph; }

signals:
    void graphReady(DependencyGraph graph);
    void graphUpdated(DependencyGraph graph);

private slots:
    void onGraphReady(DependencyGraph graph);
    void onDirectoryChanged(const QString& path);

private:
    QString m_projectPath;
    DependencyGraph m_graph;
    QThread m_workerThread;
    QFileSystemWatcher* m_watcher = nullptr;
};

#endif // DEPENDENCY_PARSER_H
