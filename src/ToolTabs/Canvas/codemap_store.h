#ifndef CODEMAP_STORE_H
#define CODEMAP_STORE_H

#include <QObject>
#include <QString>
#include <optional>
#include "codemap.h"

class CodemapStore : public QObject
{
    Q_OBJECT
public:
    explicit CodemapStore(const QString& projectRoot, QObject* parent = nullptr);

    QString defaultFilePath() const;
    bool exists() const;
    bool save(const Codemap& map);
    std::optional<Codemap> load() const;

private:
    QString m_projectRoot;
};

#endif // CODEMAP_STORE_H
