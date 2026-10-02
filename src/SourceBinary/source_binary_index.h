#ifndef SOURCE_BINARY_INDEX_H
#define SOURCE_BINARY_INDEX_H

#include <QString>
#include <QHash>
#include <QVector>
#include "../ToolTabs/Disassembler/disassemblerworker.h"

struct SourceLineMapping {
    QString filePath;
    int lineNumber = 0;
    QString revision;
    QString buildId;
    QString sectionName;
    quint64 vaddr = 0;
    quint64 vaddrEnd = 0;
    qint64 fileOffset = -1;
    QString functionName;
};

struct ObjectFileIndex {
    QString objectFilePath;
    QString sourceFilePath;
    QString buildId;
    QHash<QString, QVector<SourceLineMapping>> bySourceLine;
    // Section is part of the address identity: relocatable objects reuse offsets.
    QHash<QString, QVector<SourceLineMapping>> byVaddr;
    QHash<QString, QVector<DisasmInstruction>> instructionsBySection;
    bool indexed = false;
};

inline QString sourceAddressKey(const QString& section, quint64 address)
{
    return section + QLatin1Char(':') + QString::number(address, 16);
}

#endif // SOURCE_BINARY_INDEX_H
