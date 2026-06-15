#include "codemap_utils.h"
#include <QFile>
#include <QTextStream>
#include <QFileInfo>

QString extractCodeSnippet(const QString& projectRoot,
                           const QString& relativePath,
                           int centerLine, int contextLines)
{
    if (centerLine < 1)
        return QString();

    QString fullPath = projectRoot + "/" + relativePath;
    QFile file(fullPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();

    int startLine = qMax(1, centerLine - contextLines);
    int endLine = centerLine + contextLines;

    QTextStream in(&file);
    QStringList lines;
    int lineNum = 1;
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (lineNum >= startLine && lineNum <= endLine)
            lines.append(line);
        if (lineNum > endLine)
            break;
        lineNum++;
    }
    file.close();

    return lines.isEmpty() ? QString() : lines.join("\n");
}

bool checkLocationStale(const QString& projectRoot,
                        const CodemapLocation& location)
{
    if (location.lineNumber < 1)
        return true;

    QString fullPath = projectRoot + "/" + location.path;
    QFile file(fullPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return true;

    QTextStream in(&file);
    int lineNum = 1;
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (lineNum == location.lineNumber) {
            file.close();
            return line.trimmed() != location.lineContent.trimmed();
        }
        lineNum++;
    }

    file.close();
    return true; // lineNumber exceeds file length
}
