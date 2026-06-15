#include "codemap.h"
#include "codemap_utils.h"
#include <QJsonDocument>
#include <QFileInfo>
#include <QDir>
#include <QUuid>

// --- Path conversion ---

QString toRelativePath(const QString& absolutePath, const QString& projectRoot)
{
    if (absolutePath.startsWith(projectRoot)) {
        QString rel = absolutePath.mid(projectRoot.length());
        if (rel.startsWith('/'))
            rel = rel.mid(1);
        return rel;
    }
    return absolutePath;
}

QString toAbsolutePath(const QString& relativePath, const QString& projectRoot)
{
    if (relativePath.startsWith('/') || (relativePath.length() >= 2 && relativePath[1] == ':'))
        return relativePath;
    return projectRoot + "/" + relativePath;
}

// --- CodemapLocation ---

QJsonObject CodemapLocation::toJson() const
{
    QJsonObject obj;
    obj["id"] = id;
    obj["path"] = path;
    obj["lineNumber"] = lineNumber;
    obj["lineContent"] = lineContent;
    obj["title"] = title;
    obj["description"] = description;
    return obj;
}

CodemapLocation CodemapLocation::fromJson(const QJsonObject& obj)
{
    CodemapLocation loc;
    loc.id = obj["id"].toString();
    loc.path = obj["path"].toString();
    loc.lineNumber = obj["lineNumber"].toInt(0);
    loc.lineContent = obj["lineContent"].toString();
    loc.title = obj["title"].toString();
    loc.description = obj["description"].toString();
    return loc;
}

// --- CodemapTrace ---

QJsonObject CodemapTrace::toJson() const
{
    QJsonObject obj;
    obj["id"] = id;
    obj["title"] = title;
    obj["description"] = description;

    QJsonArray locArr;
    for (const auto& loc : locations)
        locArr.append(loc.toJson());
    obj["locations"] = locArr;

    obj["traceTextDiagram"] = traceTextDiagram;
    obj["traceGuide"] = traceGuide;
    if (!color.isEmpty())
        obj["color"] = color;

    return obj;
}

CodemapTrace CodemapTrace::fromJson(const QJsonObject& obj)
{
    CodemapTrace trace;
    trace.id = obj["id"].toString();
    trace.title = obj["title"].toString();
    trace.description = obj["description"].toString();

    if (obj.contains("locations")) {
        QJsonArray arr = obj["locations"].toArray();
        for (const auto& v : arr)
            trace.locations.append(CodemapLocation::fromJson(v.toObject()));
    }

    trace.traceTextDiagram = obj["traceTextDiagram"].toString();
    trace.traceGuide = obj["traceGuide"].toString();
    trace.color = obj["color"].toString();

    return trace;
}

QString CodemapTrace::motivationText() const
{
    // Parse "# Motivation" section from traceGuide
    QRegularExpression re("#\\s*Motivation\\s*\\n(.*?)(?:\\n#|$)",
                          QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatch match = re.match(traceGuide);
    if (match.hasMatch())
        return match.captured(1).trimmed();
    return QString();
}

QString CodemapTrace::detailsText() const
{
    QRegularExpression re("#\\s*Details\\s*\\n(.*?)(?:\\n#|$)",
                          QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatch match = re.match(traceGuide);
    if (match.hasMatch())
        return match.captured(1).trimmed();
    return QString();
}

// --- CodemapMetadata ---

QJsonObject CodemapMetadata::toJson() const
{
    QJsonObject obj;
    obj["cascadeId"] = cascadeId;
    obj["generationSource"] = generationSource;
    obj["generationTimestamp"] = generationTimestamp;
    obj["mode"] = mode;
    obj["originalPrompt"] = originalPrompt;
    return obj;
}

CodemapMetadata CodemapMetadata::fromJson(const QJsonObject& obj)
{
    CodemapMetadata meta;
    meta.cascadeId = obj["cascadeId"].toString();
    meta.generationSource = obj["generationSource"].toString();
    meta.generationTimestamp = obj["generationTimestamp"].toString();
    meta.mode = obj["mode"].toString();
    meta.originalPrompt = obj["originalPrompt"].toString();
    return meta;
}

// --- Codemap ---

QJsonObject Codemap::toJson() const
{
    QJsonObject obj;
    obj["schemaVersion"] = schemaVersion;
    obj["id"] = id;
    obj["stableId"] = stableId;
    obj["metadata"] = metadata.toJson();
    obj["title"] = title;

    QJsonArray traceArr;
    for (const auto& trace : traces)
        traceArr.append(trace.toJson());
    obj["traces"] = traceArr;

    obj["mermaidDiagram"] = mermaidDiagram;
    return obj;
}

Codemap Codemap::fromJson(const QJsonObject& obj)
{
    Codemap map;
    map.schemaVersion = obj["schemaVersion"].toInt(1);
    map.id = obj["id"].toString();
    map.stableId = obj["stableId"].toString();

    if (obj.contains("metadata"))
        map.metadata = CodemapMetadata::fromJson(obj["metadata"].toObject());

    map.title = obj["title"].toString();

    if (obj.contains("traces")) {
        QJsonArray arr = obj["traces"].toArray();
        for (const auto& v : arr)
            map.traces.append(CodemapTrace::fromJson(v.toObject()));
    }

    map.mermaidDiagram = obj["mermaidDiagram"].toString();

    // Generate stableId if empty
    if (map.stableId.isEmpty())
        map.stableId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    return map;
}

const CodemapLocation* Codemap::findLocation(const QString& locationId) const
{
    for (const auto& trace : traces) {
        for (const auto& loc : trace.locations) {
            if (loc.id == locationId)
                return &loc;
        }
    }
    return nullptr;
}

QList<Codemap::MermaidEdge> Codemap::parsedConnections() const
{
    QList<MermaidEdge> edges;
    if (mermaidDiagram.isEmpty())
        return edges;

    // Parse Mermaid edge format: "1a -->|label| 2b" or "1a --> 2b"
    QRegularExpression re(R"(\s*(\S+)\s*--[->]+\s*(?:\|([^|]*)\|)?\s*(\S+))");
    QRegularExpressionMatchIterator it = re.globalMatch(mermaidDiagram);

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        MermaidEdge edge;
        edge.from = match.captured(1);
        edge.label = match.captured(2).trimmed();
        edge.to = match.captured(3);
        edges.append(edge);
    }

    return edges;
}

void Codemap::normalizeAbsolutePaths(const QString& projectRoot)
{
    for (auto& trace : traces) {
        for (auto& loc : trace.locations) {
            loc.path = toRelativePath(loc.path, projectRoot);
        }
    }
}

void Codemap::populateRuntimeFields(const QString& projectRoot)
{
    for (auto& trace : traces) {
        for (auto& loc : trace.locations) {
            loc.codeSnippet = extractCodeSnippet(projectRoot, loc.path,
                                                  loc.lineNumber, 3);
            loc.isStale = checkLocationStale(projectRoot, loc);
        }
    }
}
