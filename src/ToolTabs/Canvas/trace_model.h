#ifndef TRACE_MODEL_H
#define TRACE_MODEL_H

#include "codemap.h"
#include "SourceBinary/source_binary_store.h"
#include <QJsonObject>
#include <QStringList>
#include <QSet>

// A small evidence model for the fixture. All address and source facts are
// loaded from tool-generated records; the GUI does not infer either.
class TraceModel
{
public:
    bool load(const QString& path, QString* error = nullptr);
    bool save(const QString& path, QString* error = nullptr) const;

    QString root() const { return m_root; }
    QString manifestPath() const { return m_manifestPath; }
    QString selectedBuildId() const { return m_document.value("selectedBuildId").toString(); }
    bool selectBuild(const QString& id);
    QJsonObject selectedBuild() const;
    QJsonObject document() const { return m_document; }
    const Codemap& codemap() const { return m_codemap; }
    const SourceBinaryStore& sourceBinary() const { return m_sourceBinary; }
    QStringList evidenceErrors() const { return m_evidenceErrors; }
    bool isSelectedBuildVerified() const { return !m_invalidBuilds.contains(selectedBuildId()); }
    QString diffText() const;
    QString sourceText(const QString& revision) const;
    QJsonArray symbolsForRevision(const QString& revision) const;
    QJsonObject symbol(const QString& symbolId, const QString& revision) const;
    QJsonObject range(const QString& symbolId) const;
    QJsonArray linksForSymbol(const QString& symbolId) const;
    QJsonArray linksForRequirement(const QString& requirementId) const;
    QStringList appearances(const QString& symbolId, const QString& revision = {}) const;

private:
    static QString fileHash(const QString& path);
    QJsonObject m_document;
    QString m_root;
    QString m_manifestPath;
    Codemap m_codemap;
    SourceBinaryStore m_sourceBinary;
    QStringList m_evidenceErrors;
    QSet<QString> m_invalidBuilds;
};

#endif
