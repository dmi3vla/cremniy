#include "agent_tools.h"

#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QTextStream>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextBrowser>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QTreeView>
#include <QFileSystemModel>
#include <QApplication>
#include <QVector>

#include "app/IDEWindow/idewindow.h"
#include "widgets/filetab.h"
#include "ui/filestabwidget.h"
#include "ui/toolstabwidget.h"
#include "ToolTabs/Canvas/canvastab.h"

// --- Helper path validation ---
bool validateAndResolvePath(const QString& projectRoot, const QString& inputPath, QString& resolvedPath)
{
    if (projectRoot.isEmpty()) return false;
    
    QFileInfo rootInfo(projectRoot);
    QString canonicalRoot = rootInfo.canonicalFilePath();
    if (canonicalRoot.isEmpty()) {
        canonicalRoot = rootInfo.absoluteFilePath();
    }
    
    QFileInfo targetInfo;
    if (QDir::isAbsolutePath(inputPath)) {
        targetInfo.setFile(inputPath);
    } else {
        targetInfo.setFile(QDir(canonicalRoot).filePath(inputPath));
    }
    
    QString cleanTarget = QDir::cleanPath(targetInfo.absoluteFilePath());
    
    // Check if within root
    if (!cleanTarget.startsWith(canonicalRoot)) {
        return false;
    }
    
    resolvedPath = cleanTarget;
    return true;
}

// --- LCS Diff Implementation ---
struct DiffLine {
    enum Type { Unchanged, Added, Removed };
    Type type;
    QString text;
};

static QList<DiffLine> computeDiff(const QStringList& oldLines, const QStringList& newLines) {
    int n = oldLines.size();
    int m = newLines.size();
    
    // DP table
    QVector<QVector<int>> dp(n + 1, QVector<int>(m + 1, 0));
    for (int i = 1; i <= n; ++i) {
        for (int j = 1; j <= m; ++j) {
            if (oldLines[i - 1] == newLines[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1] + 1;
            } else {
                dp[i][j] = qMax(dp[i - 1][j], dp[i][j - 1]);
            }
        }
    }
    
    QList<DiffLine> diff;
    int i = n, j = m;
    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && oldLines[i - 1] == newLines[j - 1]) {
            diff.prepend({DiffLine::Unchanged, oldLines[i - 1]});
            i--;
            j--;
        } else if (j > 0 && (i == 0 || dp[i][j - 1] >= dp[i - 1][j])) {
            diff.prepend({DiffLine::Added, newLines[j - 1]});
            j--;
        } else {
            diff.prepend({DiffLine::Removed, oldLines[i - 1]});
            i--;
        }
    }
    return diff;
}

// --- DiffDialog ---
DiffDialog::DiffDialog(const QString& filePath, const QString& oldContent, const QString& newContent, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("AI Agent Write Permission"));
    resize(720, 500);
    setStyleSheet("background-color: #1e1e24; color: #ffffff;");

    auto* layout = new QVBoxLayout(this);

    auto* header = new QLabel(tr("The AI Agent wants to write/modify the file:\n%1").arg(filePath), this);
    header->setStyleSheet("font-weight: bold; font-size: 13px; color: #5a8abf; padding-bottom: 6px;");
    layout->addWidget(header);

    m_diffView = new QTextBrowser(this);
    m_diffView->setStyleSheet(
        "background-color: #121216; color: #dddddd; border: 1px solid #3c3c4c; "
        "font-family: monospace; font-size: 12px; padding: 6px;"
    );

    // Compute and render diff
    QStringList oldLines = oldContent.split('\n');
    QStringList newLines = newContent.split('\n');
    QList<DiffLine> diff = computeDiff(oldLines, newLines);

    QString html = "<html><body style='margin:0; padding:0; line-height:1.4;'>";
    for (const auto& dl : diff) {
        QString esc = dl.text.toHtmlEscaped();
        if (dl.type == DiffLine::Added) {
            html += QString("<div style='background-color:#1e3d2f; color:#8ce0ad; white-space:pre-wrap;'>+ %1</div>").arg(esc);
        } else if (dl.type == DiffLine::Removed) {
            html += QString("<div style='background-color:#4a1e23; color:#f0a8b2; white-space:pre-wrap;'>- %1</div>").arg(esc);
        } else {
            html += QString("<div style='color:#aaaaaa; white-space:pre-wrap;'>  %1</div>").arg(esc);
        }
    }
    html += "</body></html>";
    m_diffView->setHtml(html);
    layout->addWidget(m_diffView, 1);

    auto* btnLayout = new QHBoxLayout();
    
    auto* rejectBtn = new QPushButton(tr("Reject"), this);
    rejectBtn->setStyleSheet(
        "QPushButton { background: #555; color: #fff; border: none; border-radius: 4px; padding: 8px 16px; font-weight: bold; }"
        "QPushButton:hover { background: #666; }"
    );
    connect(rejectBtn, &QPushButton::clicked, this, [this]() {
        m_result = Rejected;
        reject();
    });

    auto* alwaysBtn = new QPushButton(tr("Always Allow for Session"), this);
    alwaysBtn->setStyleSheet(
        "QPushButton { background: #3a5a7a; color: #fff; border: none; border-radius: 4px; padding: 8px 16px; font-weight: bold; }"
        "QPushButton:hover { background: #4a6a8a; }"
    );
    connect(alwaysBtn, &QPushButton::clicked, this, [this]() {
        m_result = AlwaysAllow;
        accept();
    });

    auto* approveBtn = new QPushButton(tr("Approve"), this);
    approveBtn->setStyleSheet(
        "QPushButton { background: #2e7d32; color: #fff; border: none; border-radius: 4px; padding: 8px 16px; font-weight: bold; }"
        "QPushButton:hover { background: #388e3c; }"
    );
    connect(approveBtn, &QPushButton::clicked, this, [this]() {
        m_result = Approved;
        accept();
    });

    btnLayout->addWidget(rejectBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(alwaysBtn);
    btnLayout->addWidget(approveBtn);

    layout->addLayout(btnLayout);
}

// --- 1. ReadFileTool ---
ReadFileTool::ReadFileTool(const QString& projectRoot, QObject* parent)
    : AgentTool(parent), m_projectRoot(projectRoot) {}

QString ReadFileTool::description() const {
    return "Read the content of a file in the project directory.";
}

QJsonObject ReadFileTool::parameters() const {
    QJsonObject params;
    params["type"] = "object";
    QJsonObject props;
    
    QJsonObject pathProp;
    pathProp["type"] = "string";
    pathProp["description"] = "The relative or absolute path of the file to read.";
    props["path"] = pathProp;
    
    params["properties"] = props;
    
    QJsonArray req;
    req.append("path");
    params["required"] = req;
    
    return params;
}

void ReadFileTool::execute(const QJsonObject& args) {
    QString path = args.value("path").toString();
    QString resolved;
    if (!validateAndResolvePath(m_projectRoot, path, resolved)) {
        emit finished("Error: Path is outside the project root or invalid", true);
        return;
    }

    QFile file(resolved);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit finished(tr("Error: Could not open file for reading: %1").arg(file.errorString()), true);
        return;
    }

    QString content = QString::fromUtf8(file.readAll());
    file.close();
    emit finished(content, false);
}

// --- 2. WriteFileTool ---
WriteFileTool::WriteFileTool(const QString& projectRoot, QObject* parent)
    : AgentTool(parent), m_projectRoot(projectRoot) {}

QString WriteFileTool::description() const {
    return "Write or overwrite the content of a file in the project directory.";
}

QJsonObject WriteFileTool::parameters() const {
    QJsonObject params;
    params["type"] = "object";
    QJsonObject props;
    
    QJsonObject pathProp;
    pathProp["type"] = "string";
    pathProp["description"] = "The relative or absolute path of the file to write.";
    props["path"] = pathProp;

    QJsonObject contentProp;
    contentProp["type"] = "string";
    contentProp["description"] = "The text content to write to the file.";
    props["content"] = contentProp;
    
    params["properties"] = props;
    
    QJsonArray req;
    req.append("path");
    req.append("content");
    params["required"] = req;
    
    return params;
}

void WriteFileTool::execute(const QJsonObject& args) {
    QString path = args.value("path").toString();
    QString content = args.value("content").toString();
    
    QString resolved;
    if (!validateAndResolvePath(m_projectRoot, path, resolved)) {
        emit finished("Error: Path is outside the project root or invalid", true);
        return;
    }

    // Read old content if file exists
    QString oldContent;
    if (QFile::exists(resolved)) {
        QFile file(resolved);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            oldContent = QString::fromUtf8(file.readAll());
            file.close();
        }
    }

    // If always allow is not set, prompt user
    if (!m_alwaysAllow) {
        DiffDialog dlg(resolved, oldContent, content, qobject_cast<QWidget*>(parent()));
        dlg.exec();
        
        if (dlg.resultType() == DiffDialog::Rejected) {
            emit finished("Error: Write operation rejected by user", true);
            return;
        } else if (dlg.resultType() == DiffDialog::AlwaysAllow) {
            m_alwaysAllow = true;
        }
    }

    // Ensure parent directories exist
    QFileInfo info(resolved);
    QDir().mkpath(info.absolutePath());

    QFile file(resolved);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        emit finished(tr("Error: Could not open file for writing: %1").arg(file.errorString()), true);
        return;
    }

    file.write(content.toUtf8());
    file.close();

    emit finished("Success: File written successfully", false);
}

// --- 3. ListDirTool ---
ListDirTool::ListDirTool(const QString& projectRoot, QObject* parent)
    : AgentTool(parent), m_projectRoot(projectRoot) {}

QString ListDirTool::description() const {
    return "List files and directories in a directory within the project.";
}

QJsonObject ListDirTool::parameters() const {
    QJsonObject params;
    params["type"] = "object";
    QJsonObject props;
    
    QJsonObject pathProp;
    pathProp["type"] = "string";
    pathProp["description"] = "The relative path to list (optional, defaults to project root).";
    props["path"] = pathProp;
    
    params["properties"] = props;
    return params;
}

void ListDirTool::execute(const QJsonObject& args) {
    QString path = args.value("path").toString();
    QString resolved;
    if (path.isEmpty()) {
        resolved = m_projectRoot;
    } else {
        if (!validateAndResolvePath(m_projectRoot, path, resolved)) {
            emit finished("Error: Path is outside the project root or invalid", true);
            return;
        }
    }

    QDir dir(resolved);
    if (!dir.exists()) {
        emit finished("Error: Directory does not exist", true);
        return;
    }

    QFileInfoList list = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    QJsonArray arr;
    for (const QFileInfo& info : list) {
        QJsonObject obj;
        obj["name"] = info.fileName();
        obj["type"] = info.isDir() ? "directory" : "file";
        obj["size"] = info.size();
        arr.append(obj);
    }

    QJsonDocument doc(arr);
    emit finished(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)), false);
}

// --- 4. SearchGrepTool ---
SearchGrepTool::SearchGrepTool(const QString& projectRoot, QObject* parent)
    : AgentTool(parent), m_projectRoot(projectRoot) {}

QString SearchGrepTool::description() const {
    return "Search for a pattern in project files (grep).";
}

QJsonObject SearchGrepTool::parameters() const {
    QJsonObject params;
    params["type"] = "object";
    QJsonObject props;
    
    QJsonObject patternProp;
    patternProp["type"] = "string";
    patternProp["description"] = "The search pattern (text substring).";
    props["pattern"] = patternProp;
    
    params["properties"] = props;
    
    QJsonArray req;
    req.append("pattern");
    params["required"] = req;
    
    return params;
}

void SearchGrepTool::execute(const QJsonObject& args) {
    QString pattern = args.value("pattern").toString();
    if (pattern.isEmpty()) {
        emit finished("Error: Pattern cannot be empty", true);
        return;
    }

    QJsonArray results;
    QDirIterator it(m_projectRoot, QDir::Files, QDirIterator::Subdirectories);
    int matchCount = 0;
    const int maxMatches = 100; // safety limit

    while (it.hasNext() && matchCount < maxMatches) {
        QString filePath = it.next();
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            int lineNum = 0;
            while (!stream.atEnd() && matchCount < maxMatches) {
                lineNum++;
                QString line = stream.readLine();
                if (line.contains(pattern, Qt::CaseInsensitive)) {
                    QJsonObject match;
                    match["file"] = QDir(m_projectRoot).relativeFilePath(filePath);
                    match["line"] = lineNum;
                    match["content"] = line.trimmed();
                    results.append(match);
                    matchCount++;
                }
            }
            file.close();
        }
    }

    QJsonDocument doc(results);
    emit finished(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)), false);
}

// --- 5. GetDependenciesTool ---
GetDependenciesTool::GetDependenciesTool(QObject* parent)
    : AgentTool(parent) {}

QString GetDependenciesTool::description() const {
    return "Get the includes/dependency graph of the project files.";
}

QJsonObject GetDependenciesTool::parameters() const {
    return QJsonObject{{"type", "object"}, {"properties", QJsonObject()}};
}

void GetDependenciesTool::execute(const QJsonObject& args) {
    Q_UNUSED(args);
    IDEWindow* ideWin = nullptr;
    for (QWidget* w : QApplication::topLevelWidgets()) {
        ideWin = qobject_cast<IDEWindow*>(w);
        if (ideWin) break;
    }
    if (!ideWin) {
        emit finished("Error: IDEWindow not found", true);
        return;
    }
    
    auto* filesTabWidget = ideWin->findChild<FilesTabWidget*>();
    if (!filesTabWidget) {
        emit finished("Error: FilesTabWidget not found", true);
        return;
    }
    
    auto* currentTab = qobject_cast<FileTab*>(filesTabWidget->currentWidget());
    if (!currentTab) {
        emit finished("Error: No file tab is currently open. Open a file first to view its canvas dependency graph.", true);
        return;
    }
    
    auto* toolsTab = currentTab->findChild<ToolsTabWidget*>();
    if (!toolsTab) {
        emit finished("Error: ToolsTabWidget not found", true);
        return;
    }
    
    auto* canvasTab = toolsTab->findChild<CanvasTab*>();
    if (!canvasTab) {
        emit finished("Error: Canvas tab not found", true);
        return;
    }
    
    DependencyGraph graph = canvasTab->currentGraph();
    
    QJsonObject resultObj;
    QJsonArray filesArr;
    for (const QString& file : graph.allFiles) {
        filesArr.append(file);
    }
    resultObj["files"] = filesArr;
    
    QJsonObject includesObj;
    for (auto it = graph.includes.begin(); it != graph.includes.end(); ++it) {
        QJsonArray inclList;
        for (const QString& inc : it->second) {
            inclList.append(inc);
        }
        includesObj[it->first] = inclList;
    }
    resultObj["dependencies"] = includesObj;
    
    QJsonDocument doc(resultObj);
    emit finished(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)), false);
}

// --- 6. OpenFileTool ---
OpenFileTool::OpenFileTool(QObject* parent)
    : AgentTool(parent) {}

QString OpenFileTool::description() const {
    return "Open a file in the IDE editor tab.";
}

QJsonObject OpenFileTool::parameters() const {
    QJsonObject params;
    params["type"] = "object";
    QJsonObject props;
    
    QJsonObject pathProp;
    pathProp["type"] = "string";
    pathProp["description"] = "The relative or absolute path of the file to open.";
    props["path"] = pathProp;
    
    params["properties"] = props;
    
    QJsonArray req;
    req.append("path");
    params["required"] = req;
    
    return params;
}

void OpenFileTool::execute(const QJsonObject& args) {
    QString path = args.value("path").toString();
    IDEWindow* ideWin = nullptr;
    for (QWidget* w : QApplication::topLevelWidgets()) {
        ideWin = qobject_cast<IDEWindow*>(w);
        if (ideWin) break;
    }
    if (!ideWin) {
        emit finished("Error: IDEWindow not found", true);
        return;
    }
    
    auto* model = qobject_cast<QFileSystemModel*>(ideWin->findChild<QTreeView*>()->model());
    QString projectRoot = model ? model->rootPath() : "";
    
    QString resolved;
    if (!validateAndResolvePath(projectRoot, path, resolved)) {
        emit finished("Error: Path is outside project root or invalid", true);
        return;
    }
    
    auto* filesTabWidget = ideWin->findChild<FilesTabWidget*>();
    if (!filesTabWidget) {
        emit finished("Error: FilesTabWidget not found", true);
        return;
    }
    
    filesTabWidget->openFile(resolved, QFileInfo(resolved).fileName());
    emit finished(QString("Successfully opened file: %1").arg(path), false);
}

// --- 7. HighlightDependenciesTool ---
HighlightDependenciesTool::HighlightDependenciesTool(QObject* parent)
    : AgentTool(parent) {}

QString HighlightDependenciesTool::description() const {
    return "Highlight dependencies of a file in the dependency canvas.";
}

QJsonObject HighlightDependenciesTool::parameters() const {
    QJsonObject params;
    params["type"] = "object";
    QJsonObject props;
    
    QJsonObject pathProp;
    pathProp["type"] = "string";
    pathProp["description"] = "The file path or name to highlight dependencies for.";
    props["path"] = pathProp;
    
    params["properties"] = props;
    
    QJsonArray req;
    req.append("path");
    params["required"] = req;
    
    return params;
}

void HighlightDependenciesTool::execute(const QJsonObject& args) {
    QString path = args.value("path").toString();
    IDEWindow* ideWin = nullptr;
    for (QWidget* w : QApplication::topLevelWidgets()) {
        ideWin = qobject_cast<IDEWindow*>(w);
        if (ideWin) break;
    }
    if (!ideWin) {
        emit finished("Error: IDEWindow not found", true);
        return;
    }
    
    auto* filesTabWidget = ideWin->findChild<FilesTabWidget*>();
    if (!filesTabWidget) {
        emit finished("Error: FilesTabWidget not found", true);
        return;
    }
    
    auto* currentTab = qobject_cast<FileTab*>(filesTabWidget->currentWidget());
    if (!currentTab) {
        emit finished("Error: No file tab is currently open. Open a file first to view its canvas dependency graph.", true);
        return;
    }
    
    auto* toolsTab = currentTab->findChild<ToolsTabWidget*>();
    if (!toolsTab) {
        emit finished("Error: ToolsTabWidget not found", true);
        return;
    }
    
    auto* canvasTab = toolsTab->findChild<CanvasTab*>();
    if (!canvasTab) {
        emit finished("Error: Canvas tab not found", true);
        return;
    }
    
    canvasTab->highlightDependencies(path);
    emit finished(QString("Highlighted dependencies for: %1").arg(path), false);
}
