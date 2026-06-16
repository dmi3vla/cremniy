#ifndef SOURCE_BINARY_STORE_H
#define SOURCE_BINARY_STORE_H

#include <QObject>
#include <QHash>
#include <QVector>
#include <optional>
#include "source_binary_index.h"

class SourceBinaryStore : public QObject
{
    Q_OBJECT
public:
    explicit SourceBinaryStore(QObject* parent = nullptr);

    void addIndex(const ObjectFileIndex& index);

    std::optional<SourceLineMapping> findBySourceLine(
        const QString& relativePath, int line) const;

    std::optional<SourceLineMapping> findByVaddr(quint64 vaddr) const;

    QVector<DisasmInstruction> instructionsInRange(
        quint64 vaddrStart, quint64 vaddrEnd) const;

    bool isEmpty() const { return m_indices.isEmpty(); }
    void clear();

private:
    QHash<QString, ObjectFileIndex> m_indices;
};

#endif // SOURCE_BINARY_STORE_H
