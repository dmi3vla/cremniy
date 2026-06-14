#include "semantic_map_store.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

SemanticMapStore::SemanticMapStore(const QString& projectRoot, QObject* parent)
    : QObject(parent)
    , m_projectRoot(projectRoot)
{
}

QString SemanticMapStore::storageDir() const
{
    return m_projectRoot + "/.cremniy/semantic_maps";
}

QString SemanticMapStore::sanitizeId(const QString& id) const
{
    QString result;
    result.reserve(id.size());
    for (const QChar& c : id) {
        if (c.isLetterOrNumber() || c == '-' || c == '_')
            result.append(c);
        else
            result.append('_');
    }
    return result;
}

bool SemanticMapStore::save(const SemanticMap& map)
{
    QDir().mkpath(storageDir());
    QString filePath = storageDir() + "/" + sanitizeId(map.id) + ".json";

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    QJsonDocument doc(map.toJson());
    file.write(doc.toJson(QJsonDocument::Compact));
    file.close();
    return true;
}

std::optional<SemanticMap> SemanticMapStore::load(const QString& id) const
{
    QString filePath = storageDir() + "/" + sanitizeId(id) + ".json";
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();

    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return std::nullopt;

    return SemanticMap::fromJson(doc.object());
}

bool SemanticMapStore::remove(const QString& id)
{
    QString filePath = storageDir() + "/" + sanitizeId(id) + ".json";
    return QFile::remove(filePath);
}

QList<SemanticMapStore::MapMeta> SemanticMapStore::list() const
{
    QList<MapMeta> result;
    QDir dir(storageDir());
    if (!dir.exists())
        return result;

    QStringList files = dir.entryList({"*.json"}, QDir::Files, QDir::Name);
    for (const QString& fileName : files) {
        QFile file(dir.filePath(fileName));
        if (!file.open(QIODevice::ReadOnly))
            continue;

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
        file.close();

        if (error.error != QJsonParseError::NoError || !doc.isObject())
            continue;

        QJsonObject obj = doc.object();
        MapMeta meta;
        meta.id = obj["id"].toString(fileName.chopped(5)); // strip .json
        meta.title = obj["title"].toString();
        meta.createdAt = QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODate);
        result.append(meta);
    }

    return result;
}
