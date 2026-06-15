#include "generate_codemap_tool.h"
#include "../llm_client.h"
#include "../endpoint_manager.h"
#include "../../ToolTabs/Canvas/codemap.h"
#include "../../ToolTabs/Canvas/codemap_store.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTextStream>
#include <QDir>
#include <QDateTime>
#include <QUuid>

GenerateCodemapTool::GenerateCodemapTool(const QString& projectRoot,
                                          EndpointManager* endpointManager,
                                          QObject* parent)
    : AgentTool(parent)
    , m_projectRoot(projectRoot)
    , m_endpointManager(endpointManager)
{
}

QString GenerateCodemapTool::description() const
{
    return "Generate a codemap in Windsurf-compatible format. "
           "Analyzes source files and produces structured traces with locations, "
           "ASCII diagrams, Mermaid connection graphs, and per-trace guides.";
}

QJsonObject GenerateCodemapTool::parameters() const
{
    QJsonObject scopeParam;
    scopeParam["type"] = "array";
    QJsonObject itemType;
    itemType["type"] = "string";
    scopeParam["items"] = itemType;
    scopeParam["description"] = "List of relative file paths to analyze. If empty, uses task.";

    QJsonObject taskParam;
    taskParam["type"] = "string";
    taskParam["description"] = "Text description of the feature/module to analyze.";

    QJsonObject titleParam;
    titleParam["type"] = "string";
    titleParam["description"] = "Optional hint for the codemap title.";

    QJsonObject props;
    props["scope"] = scopeParam;
    props["task"] = taskParam;
    props["title_hint"] = titleParam;

    QJsonArray required;
    QJsonObject result;
    result["type"] = "object";
    result["properties"] = props;
    result["required"] = required;
    return result;
}

void GenerateCodemapTool::execute(const QJsonObject& args)
{
    if (m_innerClient && m_innerClient->isBusy()) {
        emit finished("Error: another codemap generation is already in progress", true);
        return;
    }

    m_retryCount = 0;
    m_streamBuffer.clear();

    QJsonArray scopeArr = args["scope"].toArray();
    QStringList scope;
    for (const auto& v : scopeArr)
        scope.append(v.toString());

    QString task = args["task"].toString();
    QString titleHint = args["title_hint"].toString();

    if (scope.isEmpty() && task.isEmpty()) {
        emit finished("Error: either 'scope' or 'task' must be provided", true);
        return;
    }

    QString fileContext;
    if (!scope.isEmpty()) {
        fileContext = gatherFileContents(scope);
        m_availableFiles = scope;
    } else {
        fileContext = "Task: " + task + "\n\nAvailable files in project (no content provided):\n";
        m_availableFiles.clear();
    }

    m_originalSystemPrompt = buildSystemPrompt();
    m_originalUserPrompt = "Analyze the following code and generate a codemap.\n\n"
                           "## File Context\n" + fileContext;
    if (!titleHint.isEmpty())
        m_originalUserPrompt += "\n\n## Title Hint\n" + titleHint;

    sendGenerationRequest(m_originalSystemPrompt, m_originalUserPrompt);
}

QString GenerateCodemapTool::gatherFileContents(const QStringList& scope) const
{
    QString result;
    int totalChars = 0;

    for (const QString& relPath : scope) {
        QString fullPath = m_projectRoot + "/" + relPath;
        QFile file(fullPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        QString content = QTextStream(&file).readAll();
        file.close();

        // Add line numbers for reference
        QStringList numberedLines;
        QStringList lines = content.split('\n');
        for (int i = 0; i < lines.size(); ++i)
            numberedLines.append(QString::number(i + 1) + ": " + lines[i]);

        QString entry = "### " + relPath + "\n```\n" + numberedLines.join("\n") + "\n```\n\n";

        if (totalChars + entry.size() > kMaxContextChars) {
            result += "\n...[remaining files skipped due to context size limit]\n";
            break;
        }

        result += entry;
        totalChars += entry.size();
    }

    return result;
}

QString GenerateCodemapTool::buildSystemPrompt() const
{
    return R"(Ты — инструмент анализа кода, генерирующий 'codemap' — структурированную концептуальную карту проекта в формате Windsurf/Cascade codemap.

МЕТОДОЛОГИЯ (4 шага):
1. Топологический сбор контекста — определи границы анализируемого модуля по предоставленным файлам
2. AST-локализация — найди ключевые точки (entry points, конструкторы, обработчики сигналов, инициализация UI) с ТОЧНЫМИ номерами строк
3. Семантическая склейка — свяжи точки в последовательности выполнения, определи граф вызовов между точками из РАЗНЫХ traces
4. Схлопывание в traces — сгруппируй точки в 3-6 'traces' с понятными названиями

ФОРМАТ ВЫВОДА — СТРОГО JSON, без markdown-обёртки, первый символ ответа ДОЛЖЕН БЫТЬ "{":

{
  "schemaVersion": 1,
  "id": "<slug_based_on_title>",
  "stableId": "<сгенерируй случайный UUID v4>",
  "metadata": {
    "cascadeId": "",
    "generationSource": "cremniy-agent",
    "generationTimestamp": "<текущая дата ISO8601>",
    "mode": "FAST",
    "originalPrompt": "<повтори переданный task/scope>"
  },
  "title": "<человекочитаемое название>",
  "traces": [
    {
      "id": "1",
      "title": "Application Startup and Initialization",
      "description": "<1 предложение>",
      "locations": [
        {
          "id": "1a",
          "path": "<ОТНОСИТЕЛЬНЫЙ путь, ДОЛЖЕН существовать в контексте>",
          "lineNumber": <int, в пределах файла>,
          "lineContent": "<ТОЧНОЕ содержимое строки lineNumber из контекста>",
          "title": "<Qt Application Creation>",
          "description": "<1 предложение>"
        }
      ],
      "traceTextDiagram": "<ASCII-дерево>",
      "traceGuide": "# Motivation\n<текст>\n\n# Details\n<текст>",
      "color": "<hex цвет>"
    }
  ],
  "mermaidDiagram": "graph TD\n  1a -->|initializes| 2a\n  2d -->|handles| 5a"
}

КРИТИЧЕСКИЕ ТРЕБОВАНИЯ:
- path — ОТНОСИТЕЛЬНЫЙ путь, существующий в контексте. НЕ придумывай файлы.
- lineNumber — РЕАЛЬНЫЙ номер строки в пределах файла.
- lineContent — СКОПИРУЙ точное содержимое строки lineNumber из контекста (включая отступы).
- id locations: '<номер_trace><буква>' — '1a','1b','2a'...
- mermaidDiagram — синтаксис Mermaid, рёбра между id locations из РАЗНЫХ traces.
- 3-6 traces, каждый 2-8 locations.
- НЕ оборачивай JSON в markdown code fence.
- Если контекста недостаточно — верни traces: [] с объяснением в metadata.originalPrompt.)";
}

QString GenerateCodemapTool::buildUserPrompt(const QJsonObject& args) const
{
    Q_UNUSED(args)
    return "Analyze the code above and produce the codemap JSON.";
}

void GenerateCodemapTool::sendGenerationRequest(const QString& systemPrompt, const QString& userPrompt)
{
    if (!m_innerClient) {
        m_innerClient = new LLMClient(this);
        connect(m_innerClient, &LLMClient::streamToken, this, &GenerateCodemapTool::onMapStreamToken);
        connect(m_innerClient, &LLMClient::streamFinished, this, &GenerateCodemapTool::onMapStreamFinished);
        connect(m_innerClient, &LLMClient::errorOccurred, this, &GenerateCodemapTool::onMapErrorOccurred);
    }

    m_streamBuffer.clear();

    QJsonArray messages;
    QJsonObject msg;
    msg["role"] = "user";
    msg["content"] = systemPrompt + "\n\n" + userPrompt;
    messages.append(msg);

    m_innerClient->sendMessage(messages, QJsonArray{});
}

void GenerateCodemapTool::onMapStreamToken(const QString& token)
{
    m_streamBuffer += token;
}

void GenerateCodemapTool::onMapStreamFinished(const QString& stopReason)
{
    Q_UNUSED(stopReason)

    QString cleaned = m_streamBuffer.trimmed();
    if (cleaned.startsWith("```json"))
        cleaned = cleaned.mid(7);
    else if (cleaned.startsWith("```"))
        cleaned = cleaned.mid(3);
    if (cleaned.endsWith("```"))
        cleaned = cleaned.chopped(3);
    cleaned = cleaned.trimmed();

    CodemapValidationResult result = validateCodemapJson(cleaned);

    if (result.ok) {
        // Don't save here — IDEWindow handles saving with proper filename
        QJsonDocument doc(result.map.toJson());
        emit finished(doc.toJson(QJsonDocument::Compact), false);
        return;
    }

    if (m_retryCount < kMaxRetries) {
        m_retryCount++;
        QString retryPrompt = m_originalUserPrompt +
            "\n\nPREVIOUS ATTEMPT FAILED: " + result.errorReason +
            "\nPlease fix this specific issue and return corrected JSON. "
            "Remember: path must be exactly one of the provided files, "
            "lineNumber must be within the actual file length, and "
            "lineContent must exactly match the content of that line.";
        sendGenerationRequest(m_originalSystemPrompt, retryPrompt);
        return;
    }

    emit finished("Error: failed to generate valid codemap after " +
                   QString::number(kMaxRetries) + " retries. Last error: " +
                   result.errorReason, true);
}

void GenerateCodemapTool::onMapErrorOccurred(const QString& errorMessage)
{
    emit finished("Error: " + errorMessage, true);
}

CodemapValidationResult GenerateCodemapTool::validateCodemapJson(const QString& rawJson) const
{
    CodemapValidationResult vr;

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(rawJson.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        vr.errorReason = "Invalid JSON: " + parseError.errorString();
        return vr;
    }

    QJsonObject obj = doc.object();
    if (!obj.contains("title") || !obj.contains("traces")) {
        vr.errorReason = "Missing required fields (title or traces)";
        return vr;
    }

    Codemap map = Codemap::fromJson(obj);

    if (map.traces.isEmpty()) {
        vr.ok = true;
        vr.map = map;
        return vr;
    }

    // Validate locations
    for (const auto& trace : map.traces) {
        for (const auto& loc : trace.locations) {
            if (loc.id.isEmpty()) {
                vr.errorReason = "Location missing id in trace '" + trace.id + "'";
                return vr;
            }

            if (!m_availableFiles.isEmpty() && !m_availableFiles.contains(loc.path)) {
                vr.errorReason = "Location '" + loc.id + "' references unknown file: " + loc.path;
                return vr;
            }

            if (loc.lineNumber < 1) {
                vr.errorReason = "Location '" + loc.id + "' has invalid lineNumber: " +
                                 QString::number(loc.lineNumber);
                return vr;
            }

            QString fullPath = m_projectRoot + "/" + loc.path;
            int numLines = countFileLines(fullPath);
            if (numLines > 0 && loc.lineNumber > numLines) {
                vr.errorReason = "Location '" + loc.id + "' lineNumber " +
                                 QString::number(loc.lineNumber) +
                                 " exceeds file length (" + QString::number(numLines) +
                                 ") in " + loc.path;
                return vr;
            }

            // Validate lineContent matches actual file content
            if (!loc.lineContent.isEmpty()) {
                QString actualContent = readLineContent(fullPath, loc.lineNumber);
                if (!actualContent.isEmpty() &&
                    actualContent.trimmed() != loc.lineContent.trimmed()) {
                    vr.errorReason = "Location '" + loc.id +
                                     "': lineContent mismatch — model may have used wrong line number";
                    return vr;
                }
            }
        }
    }

    vr.ok = true;
    vr.map = map;
    return vr;
}

int GenerateCodemapTool::countFileLines(const QString& fullPath) const
{
    QFile file(fullPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return 0;
    int count = 0;
    QTextStream in(&file);
    while (!in.atEnd()) { in.readLine(); count++; }
    return count;
}

QString GenerateCodemapTool::readLineContent(const QString& fullPath, int lineNumber) const
{
    QFile file(fullPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    QTextStream in(&file);
    int lineNum = 1;
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (lineNum == lineNumber)
            return line;
        lineNum++;
    }
    return QString();
}
