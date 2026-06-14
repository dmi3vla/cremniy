#include "semantic_map.h"

// --- SemanticStep ---

QJsonObject SemanticStep::toJson() const
{
    QJsonObject obj;
    obj["id"] = id;
    obj["title"] = title;
    obj["filePath"] = filePath;
    obj["startLine"] = startLine;
    obj["endLine"] = endLine;
    obj["codeSnippet"] = codeSnippet;

    QJsonArray conns;
    for (const QString& c : connections)
        conns.append(c);
    obj["connections"] = conns;

    QJsonArray labels;
    for (const QString& l : connectionLabels)
        labels.append(l);
    obj["connectionLabels"] = labels;

    return obj;
}

SemanticStep SemanticStep::fromJson(const QJsonObject& obj)
{
    SemanticStep step;
    step.id = obj["id"].toString();
    step.title = obj["title"].toString();
    step.filePath = obj["filePath"].toString();
    step.startLine = obj["startLine"].toInt(0);
    step.endLine = obj["endLine"].toInt(0);
    step.codeSnippet = obj["codeSnippet"].toString();

    if (obj.contains("connections")) {
        QJsonArray conns = obj["connections"].toArray();
        for (const auto& v : conns)
            step.connections.append(v.toString());
    }
    if (obj.contains("connectionLabels")) {
        QJsonArray labels = obj["connectionLabels"].toArray();
        for (const auto& v : labels)
            step.connectionLabels.append(v.toString());
    }

    return step;
}

// --- SemanticCluster ---

QJsonObject SemanticCluster::toJson() const
{
    QJsonObject obj;
    obj["id"] = id;
    obj["title"] = title;
    obj["description"] = description;
    obj["color"] = color;

    QJsonArray arr;
    for (const auto& step : steps)
        arr.append(step.toJson());
    obj["steps"] = arr;

    return obj;
}

SemanticCluster SemanticCluster::fromJson(const QJsonObject& obj)
{
    SemanticCluster cluster;
    cluster.id = obj["id"].toString();
    cluster.title = obj["title"].toString();
    cluster.description = obj["description"].toString();
    cluster.color = obj["color"].toString();

    if (obj.contains("steps")) {
        QJsonArray arr = obj["steps"].toArray();
        for (const auto& v : arr)
            cluster.steps.append(SemanticStep::fromJson(v.toObject()));
    }

    return cluster;
}

// --- SemanticMap ---

QJsonObject SemanticMap::toJson() const
{
    QJsonObject obj;
    obj["id"] = id;
    obj["title"] = title;
    obj["motivation"] = motivation;
    obj["details"] = details;
    obj["createdAt"] = createdAt.toString(Qt::ISODate);

    QJsonArray arr;
    for (const auto& cluster : clusters)
        arr.append(cluster.toJson());
    obj["clusters"] = arr;

    return obj;
}

SemanticMap SemanticMap::fromJson(const QJsonObject& obj)
{
    SemanticMap map;
    map.id = obj["id"].toString();
    map.title = obj["title"].toString();
    map.motivation = obj["motivation"].toString();
    map.details = obj["details"].toString();
    map.createdAt = QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODate);

    if (obj.contains("clusters")) {
        QJsonArray arr = obj["clusters"].toArray();
        for (const auto& v : arr)
            map.clusters.append(SemanticCluster::fromJson(v.toObject()));
    }

    return map;
}

const SemanticStep* SemanticMap::findStep(const QString& stepId) const
{
    for (const auto& cluster : clusters) {
        for (const auto& step : cluster.steps) {
            if (step.id == stepId)
                return &step;
        }
    }
    return nullptr;
}
