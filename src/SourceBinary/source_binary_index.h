#ifndef SOURCE_BINARY_INDEX_H
#define SOURCE_BINARY_INDEX_H

#include <QString>
#include <QHash>
#include <QVector>
#include "../ToolTabs/Disassembler/disassemblerworker.h"

struct SourceLineMapping {
    QString filePath;
    int lineNumber = 0;
    quint64 vaddr = 0;
    quint64 vaddrEnd = 0;
    qint64 fileOffset = -1;
    QString functionName;
};

struct ObjectFileIndex {
    QString objectFilePath;
    QString sourceFilePath;
    QHash<QString, SourceLineMapping> bySourceLine;
    QHash<quint64, SourceLineMapping> byVaddr;
    QVector<DisasmInstruction> instructions;
    bool indexed = false;
};

#endif // SOURCE_BINARY_INDEX_H
