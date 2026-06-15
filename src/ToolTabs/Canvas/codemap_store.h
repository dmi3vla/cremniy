#ifndef CODEMAP_STORE_H
#define CODEMAP_STORE_H

#include <QObject>
#include <QString>
#include <QList>
#include <optional>
#include "codemap.h"

struct CodemapMeta {
    QString filePath;
    QString title;
    QDateTime timestamp;
};

class CodemapStore : public QObject
{
    Q_OBJECT
public:
    explicit CodemapStore(const QString& projectRoot, QObject* parent = nullptr);

    static QString slugifyTitle(const QString& title);
    static QString buildFileName(const Codemap& map);

    QString defaultFilePath() const;
    bool exists() const;
    bool save(const Codemap& map);
    std::optional<Codemap> load(const QString& filePath = QString()) const;
    std::optional<Codemap> loadLatest() const;
    QList<CodemapMeta> list() const;

private:
    QString m_projectRoot;
};

#endif // CODEMAP_STORE_H
