#ifndef SEMANTIC_MAP_STORE_H
#define SEMANTIC_MAP_STORE_H

#include <QObject>
#include <QString>
#include <QList>
#include <optional>
#include "semantic_map.h"

class SemanticMapStore : public QObject
{
    Q_OBJECT

public:
    explicit SemanticMapStore(const QString& projectRoot, QObject* parent = nullptr);

    bool save(const SemanticMap& map);
    std::optional<SemanticMap> load(const QString& id) const;
    bool remove(const QString& id);

    struct MapMeta {
        QString id;
        QString title;
        QDateTime createdAt;
    };
    QList<MapMeta> list() const;

private:
    QString m_projectRoot;
    QString storageDir() const;
    QString sanitizeId(const QString& id) const;
};

#endif // SEMANTIC_MAP_STORE_H
