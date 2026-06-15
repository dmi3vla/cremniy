#ifndef CODEMAP_UTILS_H
#define CODEMAP_UTILS_H

#include <QString>
#include "codemap.h"

QString extractCodeSnippet(const QString& projectRoot,
                           const QString& relativePath,
                           int centerLine, int contextLines = 3);

bool checkLocationStale(const QString& projectRoot,
                        const CodemapLocation& location);

#endif // CODEMAP_UTILS_H
