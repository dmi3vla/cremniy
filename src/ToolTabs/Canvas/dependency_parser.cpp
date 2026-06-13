#include "dependency_parser.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDirIterator>

DependencyParserWorker::DependencyParserWorker(const QString& projectPath, QObject* parent)
    : QObject(parent)
    , m_projectPath(projectPath)
{
}

void DependencyParserWorker::parse()
{
    DependencyGraph graph;
    QStringList files = findSourceFiles(m_projectPath);
    graph.allFiles = files;

    int total = files.size();
    for (int i = 0; i < total; ++i) {
        const QString& file = files[i];
        QStringList includes = parseIncludes(file);
        if (!includes.isEmpty()) {
            graph.includes[file] = includes;
        }
        emit progress(i + 1, total);
    }

    emit graphReady(graph);
}

QStringList DependencyParserWorker::findSourceFiles(const QString& dir) const
{
    QStringList result;
    QDirIterator it(dir, {"*.cpp", "*.h", "*.hpp", "*.c", "*.cc", "*.cxx"}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        result.append(it.next());
    }
    return result;
}

QStringList DependencyParserWorker::parseIncludes(const QString& filePath) const
{
    QStringList includes;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return includes;

    QTextStream in(&file);
    QRegularExpression re(R"(#include\s*[<"]([^>"]+)[>"])");

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        QRegularExpressionMatch match = re.match(line);
        if (match.hasMatch()) {
            QString incPath = match.captured(1);
            QString resolved = resolveInclude(incPath, filePath);
            if (!resolved.isEmpty()) {
                includes.append(resolved);
            }
        }
    }

    return includes;
}

QString DependencyParserWorker::resolveInclude(const QString& includePath, const QString& fromFile) const
{
    QFileInfo fromInfo(fromFile);
    QString dir = fromInfo.absolutePath();

    // Try relative to source file
    QString candidate = QDir(dir).filePath(includePath);
    if (QFile::exists(candidate))
        return QFileInfo(candidate).absoluteFilePath();

    // Try relative to project root
    candidate = QDir(m_projectPath).filePath(includePath);
    if (QFile::exists(candidate))
        return QFileInfo(candidate).absoluteFilePath();

    // Try src/ subdirectory
    candidate = QDir(m_projectPath).filePath("src/" + includePath);
    if (QFile::exists(candidate))
        return QFileInfo(candidate).absoluteFilePath();

    return QString();
}

DependencyParser::DependencyParser(const QString& projectPath, QObject* parent)
    : QObject(parent)
    , m_projectPath(projectPath)
{
}

void DependencyParser::startParsing()
{
    if (m_workerThread.isRunning()) {
        m_workerThread.quit();
        m_workerThread.wait();
    }

    auto* worker = new DependencyParserWorker(m_projectPath);
    worker->moveToThread(&m_workerThread);

    connect(&m_workerThread, &QThread::started, worker, &DependencyParserWorker::parse);
    connect(worker, &DependencyParserWorker::graphReady, this, &DependencyParser::onGraphReady);
    connect(worker, &DependencyParserWorker::graphReady, &m_workerThread, &QThread::quit);
    connect(&m_workerThread, &QThread::finished, worker, &QObject::deleteLater);

    m_workerThread.start();
}

void DependencyParser::watchForChanges()
{
    m_watcher = new QFileSystemWatcher(this);
    m_watcher->addPath(m_projectPath + "/src");
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &DependencyParser::onDirectoryChanged);
}

void DependencyParser::onGraphReady(DependencyGraph graph)
{
    bool isFirst = m_graph.allFiles.isEmpty();
    m_graph = graph;
    if (isFirst)
        emit graphReady(graph);
    else
        emit graphUpdated(graph);
}

void DependencyParser::onDirectoryChanged(const QString& path)
{
    Q_UNUSED(path)
    startParsing();
}
