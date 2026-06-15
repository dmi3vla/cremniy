#include "codemap_store.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

CodemapStore::CodemapStore(const QString& projectRoot, QObject* parent)
    : QObject(parent)
    , m_projectRoot(projectRoot)
{
}

QString CodemapStore::defaultFilePath() const
{
    QString projectName = QFileInfo(m_projectRoot).fileName();
    return m_projectRoot + "/" + projectName + ".codemap";
}

bool CodemapStore::exists() const
{
    return QFile::exists(defaultFilePath());
}

bool CodemapStore::save(const Codemap& map)
{
    Codemap copy = map;
    copy.normalizeAbsolutePaths(m_projectRoot);

    QFile file(defaultFilePath());
    if (!file.open(QIODevice::WriteOnly))
        return false;

    QJsonDocument doc(copy.toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

std::optional<Codemap> CodemapStore::load() const
{
    QFile file(defaultFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();

    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return std::nullopt;

    Codemap map = Codemap::fromJson(doc.object());
    map.normalizeAbsolutePaths(m_projectRoot);
    map.populateRuntimeFields(m_projectRoot);
    return map;
}
