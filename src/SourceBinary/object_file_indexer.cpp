#include "object_file_indexer.h"
#include "../ToolTabs/Canvas/codemap.h"
#include <QProcess>
#include <QRegularExpression>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

// --- ObjectFileIndexerWorker ---

ObjectFileIndexerWorker::ObjectFileIndexerWorker(const QString& r2Path, QObject* parent)
    : QObject(parent)
    , m_r2Path(r2Path)
{
}

QString ObjectFileIndexerWorker::runR2Command(const QString& objFilePath, const QString& cmd)
{
    QProcess proc;
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    proc.start(m_r2Path, {"-q", "-c", cmd, objFilePath});
    if (!proc.waitForStarted(5000))
        return QString();
    if (!proc.waitForFinished(15000))
        return QString();
    return QString::fromUtf8(proc.readAllStandardOutput());
}

quint64 ObjectFileIndexerWorker::parseHexAddress(const QString& s) const
{
    QString clean = s.trimmed();
    if (clean.startsWith("0x") || clean.startsWith("0X"))
        clean = clean.mid(2);
    return clean.toULongLong(nullptr, 16);
}

QVector<SourceLineMapping> ObjectFileIndexerWorker::parseDwarfLineInfo(const QString& objFilePath)
{
    QVector<SourceLineMapping> result;

    // r2 DWARF line info
    QString output = runR2Command(objFilePath, "e anal.dwarf.abspath=true; idpi");
    if (output.isEmpty())
        return result;

    // Parse lines like: "filepath:line vaddr"
    // or: "filepath line vaddr"
    QRegularExpression re(R"((.+?):(\d+)\s+(0x[0-9a-fA-F]+))");
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    for (const QString& line : lines) {
        QRegularExpressionMatch match = re.match(line.trimmed());
        if (!match.hasMatch())
            continue;

        SourceLineMapping mapping;
        mapping.filePath = match.captured(1).trimmed();
        mapping.lineNumber = match.captured(2).toInt();
        mapping.vaddr = parseHexAddress(match.captured(3));
        result.append(mapping);
    }

    // Sort by vaddr for vaddrEnd computation
    std::sort(result.begin(), result.end(),
              [](const SourceLineMapping& a, const SourceLineMapping& b) {
                  return a.vaddr < b.vaddr;
              });

    // Compute vaddrEnd (next line's vaddr, or vaddr+1 as fallback)
    for (int i = 0; i < result.size() - 1; ++i)
        result[i].vaddrEnd = result[i + 1].vaddr;
    if (!result.isEmpty())
        result.last().vaddrEnd = result.last().vaddr + 1;

    return result;
}

QVector<DisasmFunction> ObjectFileIndexerWorker::parseFunctions(const QString& objFilePath)
{
    QVector<DisasmFunction> result;

    QString output = runR2Command(objFilePath, "aa;afl");
    if (output.isEmpty())
        return result;

    // Parse: "0xaddr   N  funcname"
    QRegularExpression re(R"((0x[0-9a-fA-F]+)\s+\d+\s+(.+))");
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    for (const QString& line : lines) {
        QRegularExpressionMatch match = re.match(line.trimmed());
        if (!match.hasMatch())
            continue;

        DisasmFunction func;
        func.address = match.captured(1).trimmed();
        func.name = match.captured(2).trimmed();
        result.append(func);
    }

    return result;
}

QVector<DisasmSection> ObjectFileIndexerWorker::parseSections(const QString& objFilePath)
{
    QVector<DisasmSection> result;

    QString output = runR2Command(objFilePath, "iSj");
    if (output.isEmpty())
        return result;

    QJsonDocument doc = QJsonDocument::fromJson(output.toUtf8());
    if (!doc.isArray())
        return result;

    QJsonArray arr = doc.array();
    for (const auto& v : arr) {
        QJsonObject obj = v.toObject();
        DisasmSection sec;
        sec.name = obj["name"].toString();
        sec.vaddr = static_cast<quint64>(obj["vaddr"].toDouble());
        sec.fileOffset = static_cast<quint64>(obj["paddr"].toDouble());
        sec.size = static_cast<quint64>(obj["size"].toDouble());
        sec.hasFileMapping = true;
        result.append(sec);
    }

    return result;
}

void ObjectFileIndexerWorker::index(const QString& objFilePath, const QString& projectRoot)
{
    ObjectFileIndex index;
    index.objectFilePath = objFilePath;

    // Step 1: DWARF line info
    QVector<SourceLineMapping> lineMappings = parseDwarfLineInfo(objFilePath);
    if (lineMappings.isEmpty()) {
        emit indexError(objFilePath, "No DWARF line info found");
        return;
    }

    // Normalize paths
    for (auto& mapping : lineMappings) {
        mapping.filePath = toRelativePath(mapping.filePath, projectRoot);
    }

    // Step 2: Functions
    QVector<DisasmFunction> functions = parseFunctions(objFilePath);

    // Step 3: Sections for fileOffset computation
    QVector<DisasmSection> sections = parseSections(objFilePath);

    // Step 4: Find function owner for each mapping
    for (auto& mapping : lineMappings) {
        for (const auto& func : functions) {
            quint64 funcAddr = parseHexAddress(func.address);
            // Simple heuristic: mapping belongs to nearest preceding function
            if (funcAddr <= mapping.vaddr) {
                if (mapping.functionName.isEmpty() ||
                    funcAddr > parseHexAddress(functions.isEmpty() ? "0" :
                        [this, &functions, &mapping]() {
                            for (const auto& f : functions)
                                if (f.name == mapping.functionName)
                                    return f.address;
                            return QString("0");
                        }())) {
                    mapping.functionName = func.name;
                }
            }
        }
    }

    // Step 5: Compute fileOffset from vaddr using sections
    for (auto& mapping : lineMappings) {
        for (const auto& sec : sections) {
            if (mapping.vaddr >= sec.vaddr && mapping.vaddr < sec.vaddr + sec.size) {
                mapping.fileOffset = static_cast<qint64>(sec.fileOffset + (mapping.vaddr - sec.vaddr));
                break;
            }
        }
    }

    // Step 6: Build indices
    for (const auto& mapping : lineMappings) {
        QString key = mapping.filePath + ":" + QString::number(mapping.lineNumber);
        index.bySourceLine[key] = mapping;
        index.byVaddr[mapping.vaddr] = mapping;
    }

    // Step 7: Get instructions (reuse r2)
    QString disasmOutput = runR2Command(objFilePath, "aa;pdf");
    // Instructions are secondary — focus on line mappings for now

    index.indexed = true;
    if (!lineMappings.isEmpty())
        index.sourceFilePath = lineMappings.first().filePath;

    emit indexReady(index);
}

// --- ObjectFileIndexer (thread wrapper) ---

ObjectFileIndexer::ObjectFileIndexer(const QString& r2Path, QObject* parent)
    : QObject(parent)
    , m_r2Path(r2Path)
{
}

ObjectFileIndexer::~ObjectFileIndexer()
{
    if (m_workerThread.isRunning()) {
        m_workerThread.quit();
        m_workerThread.wait();
    }
}

void ObjectFileIndexer::indexObjectFile(const QString& objFilePath, const QString& projectRoot)
{
    if (m_workerThread.isRunning()) {
        m_workerThread.quit();
        m_workerThread.wait();
    }

    auto* worker = new ObjectFileIndexerWorker(m_r2Path);
    worker->moveToThread(&m_workerThread);

    connect(&m_workerThread, &QThread::started, worker, [worker, objFilePath, projectRoot]() {
        worker->index(objFilePath, projectRoot);
    });
    connect(worker, &ObjectFileIndexerWorker::indexReady, this, &ObjectFileIndexer::indexReady);
    connect(worker, &ObjectFileIndexerWorker::indexError, this, &ObjectFileIndexer::indexError);
    connect(worker, &ObjectFileIndexerWorker::indexReady, &m_workerThread, &QThread::quit);
    connect(worker, &ObjectFileIndexerWorker::indexError, &m_workerThread, &QThread::quit);
    connect(&m_workerThread, &QThread::finished, worker, &QObject::deleteLater);

    m_workerThread.start();
}
