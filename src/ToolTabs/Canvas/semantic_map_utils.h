#ifndef SEMANTIC_MAP_UTILS_H
#define SEMANTIC_MAP_UTILS_H

#include <QString>

QString extractCodeSnippet(const QString& projectRoot,
                           const QString& relativeFilePath,
                           int startLine, int endLine,
                           int contextLines = 0);

#endif // SEMANTIC_MAP_UTILS_H
