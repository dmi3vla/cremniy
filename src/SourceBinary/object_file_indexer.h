#ifndef OBJECT_FILE_INDEXER_H
#define OBJECT_FILE_INDEXER_H

#include <QObject>
#include <QThread>
#include "source_binary_index.h"

class ObjectFileIndexerWorker : public QObject
{
    Q_OBJECT
public:
    explicit ObjectFileIndexerWorker(const QString& r2Path, QObject* parent = nullptr);

public slots:
    void index(const QString& objFilePath, const QString& projectRoot);

signals:
    void indexReady(const ObjectFileIndex& index);
    void indexError(const QString& objFilePath, const QString& error);

private:
    QString m_r2Path;

    QVector<SourceLineMapping> parseDwarfLineInfo(const QString& objFilePath);
    QVector<DisasmFunction> parseFunctions(const QString& objFilePath);
    QVector<DisasmSection> parseSections(const QString& objFilePath);
    QString runR2Command(const QString& objFilePath, const QString& cmd);
    quint64 parseHexAddress(const QString& s) const;
};

class ObjectFileIndexer : public QObject
{
    Q_OBJECT
public:
    explicit ObjectFileIndexer(const QString& r2Path, QObject* parent = nullptr);
    ~ObjectFileIndexer();

    void indexObjectFile(const QString& objFilePath, const QString& projectRoot);

signals:
    void indexReady(const ObjectFileIndex& index);
    void indexError(const QString& objFilePath, const QString& error);

private:
    QString m_r2Path;
    QThread m_workerThread;
};

#endif // OBJECT_FILE_INDEXER_H
