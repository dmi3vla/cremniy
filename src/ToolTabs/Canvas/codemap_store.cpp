#include "codemap_store.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QRegularExpression>

CodemapStore::CodemapStore(const QString& projectRoot, QObject* parent)
    : QObject(parent)
    , m_projectRoot(projectRoot)
{
}

QString CodemapStore::slugifyTitle(const QString& title)
{
    QString result;
    bool lastWasUnderscore = true; // start true to skip leading underscores
    for (const QChar& c : title.trimmed()) {
        if (c.isLetterOrNumber()) {
            result.append(c);
            lastWasUnderscore = false;
        } else if ((c == ' ' || c == '-' || c == '_') && !lastWasUnderscore) {
            result.append('_');
            lastWasUnderscore = true;
        }
    }
    while (result.endsWith('_'))
        result.chop(1);
    return result;
}

QString CodemapStore::buildFileName(const Codemap& map)
{
    QString slug = slugifyTitle(map.title);
    if (slug.isEmpty())
        slug = "codemap";

    QString timestamp;
    if (!map.metadata.generationTimestamp.isEmpty()) {
        QDateTime dt = QDateTime::fromString(map.metadata.generationTimestamp, Qt::ISODate);
        if (dt.isValid())
            timestamp = dt.toString("yyyyMMdd_HHmmss");
    }
    if (timestamp.isEmpty())
        timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");

    return slug + "_" + timestamp + ".codemap.txt";
}

QString CodemapStore::defaultFilePath() const
{
    return m_projectRoot + "/" + buildFileName(Codemap{});
}

bool CodemapStore::exists() const
{
    // Check for new-style *.codemap.txt files
    QDir dir(m_projectRoot);
    QStringList filters = {"*.codemap.txt"};
    if (!dir.entryList(filters, QDir::Files).isEmpty())
        return true;

    // Check for legacy <project>.codemap
    QString projectName = QFileInfo(m_projectRoot).fileName();
    return QFile::exists(m_projectRoot + "/" + projectName + ".codemap");
}

bool CodemapStore::save(const Codemap& map)
{
    Codemap copy = map;
    copy.normalizeAbsolutePaths(m_projectRoot);

    QString filePath = m_projectRoot + "/" + buildFileName(copy);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    QJsonDocument doc(copy.toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

std::optional<Codemap> CodemapStore::load(const QString& filePath) const
{
    QString path = filePath;
    if (path.isEmpty())
        path = m_projectRoot + "/" + buildFileName(Codemap{});

    QFile file(path);
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

std::optional<Codemap> CodemapStore::loadLatest() const
{
    auto metaList = list();
    if (metaList.isEmpty())
        return std::nullopt;

    // Sort by timestamp descending, load the newest
    std::sort(metaList.begin(), metaList.end(),
              [](const CodemapMeta& a, const CodemapMeta& b) {
                  return a.timestamp > b.timestamp;
              });

    return load(metaList.first().filePath);
}

QList<CodemapMeta> CodemapStore::list() const
{
    QList<CodemapMeta> result;
    QDir dir(m_projectRoot);

    // New-style *.codemap.txt files
    QStringList filters = {"*.codemap.txt"};
    QStringList files = dir.entryList(filters, QDir::Files, QDir::Name);
    for (const QString& fileName : files) {
        QString fullPath = dir.filePath(fileName);
        QFile file(fullPath);
        if (!file.open(QIODevice::ReadOnly))
            continue;

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
        file.close();

        if (error.error != QJsonParseError::NoError || !doc.isObject())
            continue;

        QJsonObject obj = doc.object();
        CodemapMeta meta;
        meta.filePath = fullPath;
        meta.title = obj["title"].toString(fileName);
        meta.timestamp = QDateTime::fromString(
            obj["metadata"].toObject()["generationTimestamp"].toString(), Qt::ISODate);
        if (!meta.timestamp.isValid())
            meta.timestamp = QFileInfo(fullPath).lastModified();
        result.append(meta);
    }

    // Legacy <project>.codemap
    QString projectName = QFileInfo(m_projectRoot).fileName();
    QString legacyPath = m_projectRoot + "/" + projectName + ".codemap";
    if (QFile::exists(legacyPath)) {
        QFile file(legacyPath);
        if (file.open(QIODevice::ReadOnly)) {
            QJsonParseError error;
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
            file.close();
            if (error.error == QJsonParseError::NoError && doc.isObject()) {
                QJsonObject obj = doc.object();
                CodemapMeta meta;
                meta.filePath = legacyPath;
                meta.title = obj["title"].toString(projectName);
                meta.timestamp = QDateTime::fromString(
                    obj["metadata"].toObject()["generationTimestamp"].toString(), Qt::ISODate);
                if (!meta.timestamp.isValid())
                    meta.timestamp = QFileInfo(legacyPath).lastModified();
                result.append(meta);
            }
        }
    }

    return result;
}
