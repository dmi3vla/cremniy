#include "trace_model.h"
#include "codemap_store.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>

QString TraceModel::fileHash(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QString::fromLatin1(QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex());
}

bool TraceModel::load(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = parseError.errorString();
        return false;
    }
    const auto object = document.object();
    if (object.value("schemaVersion").toInt() != 1 ||
        !object.value("builds").isArray() || !object.value("symbols").isArray() ||
        !object.value("links").isArray()) {
        if (error) *error = QStringLiteral("Unsupported trace manifest");
        return false;
    }
    m_document = object;
    m_manifestPath = QFileInfo(path).absoluteFilePath();
    m_root = object.value("evidenceRoot").toString();
    if (m_root.isEmpty()) m_root = QFileInfo(path).absolutePath();
    m_sourceBinary.clear();
    m_evidenceErrors.clear();
    m_invalidBuilds.clear();

    CodemapStore codemapStore(m_root);
    const auto codemap = codemapStore.load(QDir(m_root).filePath(object.value("codemap").toString()));
    m_codemap = codemap.value_or(Codemap{});
    if (!codemap) m_evidenceErrors << QStringLiteral("Codemap missing or invalid");

    const auto revisions = object.value("revisions").toObject();
    QSet<QString> invalidRevisions;
    for (auto it = revisions.begin(); it != revisions.end(); ++it) {
        const auto rev = it.value().toObject();
        const auto source = QDir(m_root).filePath(rev.value("source").toString());
        if (fileHash(source) != rev.value("sha256").toString()) {
            m_evidenceErrors << QStringLiteral("Source hash mismatch: ") + it.key();
            invalidRevisions.insert(it.key());
        }
    }

    for (const auto& value : object.value("builds").toArray()) {
        const auto build = value.toObject();
        const QString buildId = build.value("id").toString();
        const QString revisionId = build.value("revision").toString();
        if (invalidRevisions.contains(revisionId)) {
            m_invalidBuilds.insert(buildId);
            continue;
        }
        const QString binary = QDir(m_root).filePath(build.value("binary").toString());
        if (fileHash(binary) != build.value("binarySha256").toString()) {
            m_evidenceErrors << QStringLiteral("Binary hash mismatch: ") + buildId;
            m_invalidBuilds.insert(buildId);
            continue; // Never present stale addresses as evidence.
        }
        const QString disassembly = QDir(m_root).filePath(build.value("disassembly").toString());
        if (fileHash(disassembly) != build.value("disassemblySha256").toString()) {
            m_evidenceErrors << QStringLiteral("Disassembly hash mismatch: ") + buildId;
            m_invalidBuilds.insert(buildId);
            continue;
        }
        const auto revision = revisions.value(revisionId).toObject();
        if (build.value("sourceSha256").toString() != revision.value("sha256").toString()) {
            m_evidenceErrors << QStringLiteral("Build/source revision mismatch: ") + buildId;
            m_invalidBuilds.insert(buildId);
            continue;
        }
        ObjectFileIndex index;
        index.objectFilePath = binary;
        index.buildId = buildId;
        index.indexed = true;
        for (const auto& linkValue : build.value("sourceLinks").toArray()) {
            const auto link = linkValue.toObject();
            const auto source = link.value("source").toObject();
            const auto target = link.value("binary").toObject();
            if (link.value("kind").toString() != "source-instruction" ||
                link.value("originKind").toString() != "tool" ||
                link.value("buildId").toString() != buildId ||
                target.value("buildId").toString() != buildId ||
                source.value("revision").toString() != build.value("revision").toString() ||
                source.value("sourceSha256").toString() != build.value("sourceSha256").toString()) {
                m_evidenceErrors << QStringLiteral("Invalid source/binary link in ") + buildId;
                continue;
            }
            bool startOk = false;
            bool endOk = false;
            const quint64 start = target.value("start").toString().toULongLong(&startOk, 0);
            const quint64 end = target.value("end").toString().toULongLong(&endOk, 0);
            if (!startOk || !endOk || end <= start || target.value("section").toString().isEmpty()) {
                m_evidenceErrors << QStringLiteral("Invalid instruction interval in ") + buildId;
                continue;
            }
            SourceLineMapping mapping;
            mapping.filePath = source.value("file").toString();
            mapping.lineNumber = source.value("startLine").toInt();
            mapping.revision = source.value("revision").toString();
            mapping.buildId = buildId;
            mapping.sectionName = target.value("section").toString();
            mapping.vaddr = start;
            mapping.vaddrEnd = end;
            mapping.functionName = link.value("symbolId").toString();
            index.bySourceLine[mapping.filePath + ':' + QString::number(mapping.lineNumber)].append(mapping);
            index.byVaddr[sourceAddressKey(mapping.sectionName, start)].append(mapping);
        }
        m_sourceBinary.addIndex(index);
    }
    if (!selectedBuild().contains("id") && !object.value("builds").toArray().isEmpty())
        m_document["selectedBuildId"] = object.value("builds").toArray().first().toObject().value("id");
    return true;
}

bool TraceModel::save(const QString& path, QString* error) const
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    QJsonObject saved = m_document;
    saved["evidenceRoot"] = m_root;
    file.write(QJsonDocument(saved).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

bool TraceModel::selectBuild(const QString& id)
{
    for (const auto& value : m_document.value("builds").toArray()) {
        if (value.toObject().value("id").toString() == id) {
            m_document["selectedBuildId"] = id;
            return true;
        }
    }
    return false;
}

QJsonObject TraceModel::selectedBuild() const
{
    for (const auto& value : m_document.value("builds").toArray()) {
        const auto build = value.toObject();
        if (build.value("id").toString() == selectedBuildId()) return build;
    }
    return {};
}

QString TraceModel::diffText() const
{
    QFile file(QDir(m_root).filePath(m_document.value("diff").toString()));
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

QString TraceModel::sourceText(const QString& revision) const
{
    const auto rev = m_document.value("revisions").toObject().value(revision).toObject();
    QFile file(QDir(m_root).filePath(rev.value("source").toString()));
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

QJsonArray TraceModel::symbolsForRevision(const QString& revision) const
{
    QJsonArray result;
    for (const auto& value : m_document.value("symbols").toArray())
        if (value.toObject().value("revision").toString() == revision) result.append(value);
    return result;
}

QJsonObject TraceModel::symbol(const QString& symbolId, const QString& revision) const
{
    for (const auto& value : symbolsForRevision(revision)) {
        const auto item = value.toObject();
        if (item.value("id").toString() == symbolId) return item;
    }
    return {};
}

QJsonObject TraceModel::range(const QString& symbolId) const
{
    if (!isSelectedBuildVerified()) return {{"status", "unverified-evidence"}};
    for (const auto& value : selectedBuild().value("ranges").toArray()) {
        const auto item = value.toObject();
        if (item.value("symbolId").toString() == symbolId) return item;
    }
    return {};
}

QJsonArray TraceModel::linksForSymbol(const QString& symbolId) const
{
    QJsonArray result;
    for (const auto& value : m_document.value("links").toArray()) {
        const auto link = value.toObject();
        if (link.value("symbolId").toString() == symbolId ||
            link.value("from").toString() == symbolId || link.value("to").toString() == symbolId)
            result.append(value);
    }
    return result;
}

QJsonArray TraceModel::linksForRequirement(const QString& requirementId) const
{
    QJsonArray result;
    for (const auto& value : m_document.value("links").toArray()) {
        const auto link = value.toObject();
        if (link.value("requirementId").toString() == requirementId) result.append(value);
    }
    return result;
}

QStringList TraceModel::appearances(const QString& symbolId, const QString& revision) const
{
    QStringList result;
    const auto name = symbolId.section("::", 1, 1).section('(', 0, 0);
    for (const auto& trace : m_codemap.traces) {
        for (const auto& location : trace.locations) {
            if (!revision.isEmpty() && !location.path.startsWith(revision + '/')) continue;
            if (location.title == name)
                result << trace.id + ':' + location.id + " @ " + location.path;
        }
    }
    return result;
}
