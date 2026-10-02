#include "object_file_indexer.h"
#include "../ToolTabs/Canvas/codemap.h"
#include <QProcess>
#include <QRegularExpression>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QFile>

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

    // A DWARF row identifies an address, not a function extent. Do not infer
    // ownership or a range from the next row, particularly for .o sections.
    for (auto& mapping : result)
        mapping.vaddrEnd = mapping.vaddr + 1;

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
    QFile binary(objFilePath);
    if (binary.open(QIODevice::ReadOnly))
        index.buildId = QString::fromLatin1(
            QCryptographicHash::hash(binary.readAll(), QCryptographicHash::Sha256).toHex());

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

    // Step 2: Sections for fileOffset computation
    QVector<DisasmSection> sections = parseSections(objFilePath);

    // Step 3: A raw DWARF address without a section is ambiguous if sections
    // overlap. Keep one record per candidate section; do not guess a symbol.
    QVector<SourceLineMapping> sectionMappings;
    for (const auto& raw : lineMappings) {
        bool found = false;
        for (const auto& sec : sections) {
            if (raw.vaddr >= sec.vaddr && raw.vaddr - sec.vaddr < sec.size) {
                SourceLineMapping mapping = raw;
                mapping.sectionName = sec.name;
                mapping.buildId = index.buildId;
                mapping.fileOffset = static_cast<qint64>(sec.fileOffset + (raw.vaddr - sec.vaddr));
                sectionMappings.append(mapping);
                found = true;
            }
        }
        if (!found) {
            SourceLineMapping mapping = raw;
            mapping.buildId = index.buildId;
            sectionMappings.append(mapping);
        }
    }

    // Step 4: Preserve all rows, including repeated source lines and addresses.
    for (const auto& mapping : sectionMappings) {
        QString key = mapping.filePath + ":" + QString::number(mapping.lineNumber);
        index.bySourceLine[key].append(mapping);
        index.byVaddr[sourceAddressKey(mapping.sectionName, mapping.vaddr)].append(mapping);
    }

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
