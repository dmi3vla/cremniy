#ifndef SEMANTIC_MAP_H
#define SEMANTIC_MAP_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>

struct SemanticStep {
    QString id;
    QString title;
    QString filePath;
    int startLine = 0;
    int endLine = 0;
    QString codeSnippet;
    QStringList connections;
    QStringList connectionLabels;

    QJsonObject toJson() const;
    static SemanticStep fromJson(const QJsonObject& obj);
};

struct SemanticCluster {
    QString id;
    QString title;
    QString description;
    QString color;
    QList<SemanticStep> steps;

    QJsonObject toJson() const;
    static SemanticCluster fromJson(const QJsonObject& obj);
};

struct SemanticMap {
    QString id;
    QString title;
    QString motivation;
    QString details;
    QDateTime createdAt;
    QList<SemanticCluster> clusters;

    QJsonObject toJson() const;
    static SemanticMap fromJson(const QJsonObject& obj);
    const SemanticStep* findStep(const QString& stepId) const;
};

#endif // SEMANTIC_MAP_H
