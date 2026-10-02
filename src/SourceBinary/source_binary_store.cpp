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
    const auto matches = findAllBySourceLine(relativePath, line);
    if (matches.size() == 1) return matches.first();
    return std::nullopt;
}

QVector<SourceLineMapping> SourceBinaryStore::findAllBySourceLine(
    const QString& relativePath, int line, const QString& buildId) const
{
    const QString key = relativePath + ':' + QString::number(line);
    QVector<SourceLineMapping> result;
    for (const auto& index : m_indices) {
        if (!buildId.isEmpty() && index.buildId != buildId) continue;
        result += index.bySourceLine.value(key);
    }
    return result;
}

std::optional<SourceLineMapping> SourceBinaryStore::findByVaddr(quint64 vaddr) const
{
    const auto matches = findAllByVaddr(vaddr);
    if (matches.size() == 1) return matches.first();
    return std::nullopt;
}

QVector<SourceLineMapping> SourceBinaryStore::findAllByVaddr(
    const QString& section, quint64 vaddr, const QString& buildId) const
{
    QVector<SourceLineMapping> result;
    const QString key = sourceAddressKey(section, vaddr);
    for (const auto& index : m_indices) {
        if (!buildId.isEmpty() && index.buildId != buildId) continue;
        result += index.byVaddr.value(key);
    }
    return result;
}

QVector<SourceLineMapping> SourceBinaryStore::findAllByVaddr(
    quint64 vaddr, const QString& buildId) const
{
    QVector<SourceLineMapping> result;
    for (const auto& index : m_indices) {
        if (!buildId.isEmpty() && index.buildId != buildId) continue;
        for (const auto& mappings : index.byVaddr) {
            for (const auto& mapping : mappings) {
                if (mapping.vaddr == vaddr) result.append(mapping);
            }
        }
    }
    return result;
}

QVector<DisasmInstruction> SourceBinaryStore::instructionsInRange(
    const QString& buildId, const QString& section,
    quint64 vaddrStart, quint64 vaddrEnd) const
{
    QVector<DisasmInstruction> result;

    for (const auto& index : m_indices) {
        if (index.buildId != buildId) continue;
        for (const auto& instr : index.instructionsBySection.value(section)) {
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
