#ifndef CODEMAP_H
#define CODEMAP_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>

struct CodemapLocation {
    QString id;
    QString path;
    int lineNumber = 0;
    QString lineContent;
    QString title;
    QString description;

    QString codeSnippet;
    bool isStale = false;

    QJsonObject toJson() const;
    static CodemapLocation fromJson(const QJsonObject& obj);
};

struct CodemapTrace {
    QString id;
    QString title;
    QString description;
    QList<CodemapLocation> locations;
    QString traceTextDiagram;
    QString traceGuide;
    QString color;

    QJsonObject toJson() const;
    static CodemapTrace fromJson(const QJsonObject& obj);

    QString motivationText() const;
    QString detailsText() const;
};

struct CodemapMetadata {
    QString cascadeId;
    QString generationSource;
    QString generationTimestamp;
    QString mode;
    QString originalPrompt;

    QJsonObject toJson() const;
    static CodemapMetadata fromJson(const QJsonObject& obj);
};

struct Codemap {
    int schemaVersion = 1;
    QString id;
    QString stableId;
    CodemapMetadata metadata;
    QString title;
    QList<CodemapTrace> traces;
    QString mermaidDiagram;

    QJsonObject toJson() const;
    static Codemap fromJson(const QJsonObject& obj);
    const CodemapLocation* findLocation(const QString& locationId) const;

    struct MermaidEdge { QString from; QString to; QString label; };
    QList<MermaidEdge> parsedConnections() const;

    void normalizeAbsolutePaths(const QString& projectRoot);
    void populateRuntimeFields(const QString& projectRoot);
};

QString toRelativePath(const QString& absolutePath, const QString& projectRoot);
QString toAbsolutePath(const QString& relativePath, const QString& projectRoot);

#endif // CODEMAP_H
