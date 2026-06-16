#include "source_binary_store.h"

SourceBinaryStore::SourceBinaryStore(QObject* parent)
    : QObject(parent)
{
}

void SourceBinaryStore::addIndex(const ObjectFileIndex& index)
{
    m_indices[index.objectFilePath] = index;
}

std::optional<SourceLineMapping> SourceBinaryStore::findBySourceLine(
    const QString& relativePath, int line) const
{
    QString key = relativePath + ":" + QString::number(line);

    for (auto it = m_indices.begin(); it != m_indices.end(); ++it) {
        auto found = it.value().bySourceLine.find(key);
        if (found != it.value().bySourceLine.end())
            return *found;
    }

    return std::nullopt;
}

std::optional<SourceLineMapping> SourceBinaryStore::findByVaddr(quint64 vaddr) const
{
    for (auto it = m_indices.begin(); it != m_indices.end(); ++it) {
        auto found = it.value().byVaddr.find(vaddr);
        if (found != it.value().byVaddr.end())
            return *found;
    }

    return std::nullopt;
}

QVector<DisasmInstruction> SourceBinaryStore::instructionsInRange(
    quint64 vaddrStart, quint64 vaddrEnd) const
{
    QVector<DisasmInstruction> result;

    for (auto it = m_indices.begin(); it != m_indices.end(); ++it) {
        for (const auto& instr : it.value().instructions) {
            // Parse address string to quint64
            bool ok;
            quint64 addr = instr.address.toULongLong(&ok, 16);
            if (ok && addr >= vaddrStart && addr < vaddrEnd)
                result.append(instr);
        }
    }

    return result;
}

void SourceBinaryStore::clear()
{
    m_indices.clear();
}
