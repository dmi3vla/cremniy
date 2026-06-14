#include "semantic_map_utils.h"
#include <QFile>
#include <QTextStream>
#include <QFileInfo>

QString extractCodeSnippet(const QString& projectRoot,
                           const QString& relativeFilePath,
                           int startLine, int endLine,
                           int contextLines)
{
    if (startLine < 1 || endLine < startLine)
        return QString();

    QString fullPath = projectRoot + "/" + relativeFilePath;
    QFile file(fullPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();

    QTextStream in(&file);
    int actualStart = qMax(1, startLine - contextLines);
    int actualEnd = endLine + contextLines;

    QStringList lines;
    int lineNum = 1;
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (lineNum >= actualStart && lineNum <= actualEnd)
            lines.append(line);
        if (lineNum > actualEnd)
            break;
        lineNum++;
    }

    file.close();

    if (lines.isEmpty())
        return QString();

    return lines.join("\n");
}
