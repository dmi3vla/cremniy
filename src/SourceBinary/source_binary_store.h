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

    QVector<SourceLineMapping> findAllBySourceLine(
        const QString& relativePath, int line, const QString& buildId = {}) const;

    std::optional<SourceLineMapping> findByVaddr(quint64 vaddr) const;

    QVector<SourceLineMapping> findAllByVaddr(
        const QString& section, quint64 vaddr, const QString& buildId = {}) const;

    QVector<SourceLineMapping> findAllByVaddr(quint64 vaddr,
                                               const QString& buildId = {}) const;

    QVector<DisasmInstruction> instructionsInRange(
        const QString& buildId, const QString& section,
        quint64 vaddrStart, quint64 vaddrEnd) const;

    bool isEmpty() const { return m_indices.isEmpty(); }
    void clear();

private:
    QHash<QString, ObjectFileIndex> m_indices;
};

#endif // SOURCE_BINARY_STORE_H
