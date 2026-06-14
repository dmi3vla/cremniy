#include "generate_semantic_map_tool.h"
#include "../llm_client.h"
#include "../endpoint_manager.h"
#include "../../ToolTabs/Canvas/semantic_map.h"
#include "../../ToolTabs/Canvas/semantic_map_utils.h"
#include "../../ToolTabs/Canvas/semantic_map_store.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTextStream>
#include <QDir>

GenerateSemanticMapTool::GenerateSemanticMapTool(const QString& projectRoot,
                                                   EndpointManager* endpointManager,
                                                   QObject* parent)
    : AgentTool(parent)
    , m_projectRoot(projectRoot)
    , m_endpointManager(endpointManager)
{
}

QString GenerateSemanticMapTool::description() const
{
    return "Generate a conceptual semantic map of a code module or feature. "
           "Analyzes source files and produces a structured map with clusters "
           "of related functionality, entry points, and execution flows. "
           "Uses its own internal LLM call (isolated from main conversation).";
}

QJsonObject GenerateSemanticMapTool::parameters() const
{
    QJsonObject stringType;
    stringType["type"] = "string";

    QJsonObject itemType;
    itemType["type"] = "string";

    QJsonObject scopeParam;
    scopeParam["type"] = "array";
    scopeParam["items"] = itemType;
    scopeParam["description"] = "List of relative file paths to analyze. If empty, uses task.";

    QJsonObject taskParam;
    taskParam["type"] = "string";
    taskParam["description"] = "Text description of the feature/module to analyze, if specific files are not provided.";

    QJsonObject titleParam;
    titleParam["type"] = "string";
    titleParam["description"] = "Optional hint for the map title.";

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

void GenerateSemanticMapTool::execute(const QJsonObject& args)
{
    if (m_innerClient && m_innerClient->isBusy()) {
        emit finished("Error: another semantic map generation is already in progress", true);
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

    // Gather file contents and build available files list
    QString fileContext;
    if (!scope.isEmpty()) {
        fileContext = gatherFileContents(scope);
        m_availableFiles = scope;
    } else {
        // task-only mode: list files without content
        fileContext = "Task: " + task + "\n\nAvailable files in project (no content provided):\n";
        // We don't have DependencyGraph access here, so just note it
        fileContext += "(Agent should use read_file to examine relevant files)\n";
        m_availableFiles.clear(); // can't validate paths in task-only mode
    }

    m_originalSystemPrompt = buildSystemPrompt();
    m_originalUserPrompt = "Analyze the following code and generate a semantic map.\n\n"
                           "## File Context\n" + fileContext;
    if (!titleHint.isEmpty())
        m_originalUserPrompt += "\n\n## Title Hint\n" + titleHint;

    sendGenerationRequest(m_originalSystemPrompt, m_originalUserPrompt);
}

QString GenerateSemanticMapTool::gatherFileContents(const QStringList& scope) const
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

        QString entry = "### " + relPath + "\n```\n" + content + "\n```\n\n";

        if (totalChars + entry.size() > kMaxContextChars) {
            result += "\n...[remaining files skipped due to context size limit]\n";
            break;
        }

        result += entry;
        totalChars += entry.size();
    }

    return result;
}

QString GenerateSemanticMapTool::buildSystemPrompt() const
{
    return R"(Ты — инструмент анализа кода, который строит концептуальную карту (semantic map) проекта в стиле инструментов code review (Windsurf/Cascade codemap). В отличие от структурного графа зависимостей (#include), твоя задача — выделить ЛОГИЧЕСКИЕ этапы и группы функциональности.

МЕТОДОЛОГИЯ (4 шага):
1. Определи границы модуля/фичи — какие файлы реально относятся к анализируемой области.
2. Найди ключевые точки (entry points, конструкторы, обработчики сигналов, точки инициализации UI).
3. Свяжи точки в последовательности выполнения (что вызывает что, в каком порядке происходит инициализация/обработка событий).
4. Сгруппируй связанные точки в 3-6 кластеров с понятными названиями.

ФОРМАТ ВЫВОДА — СТРОГО JSON, без markdown-разметки, без ```json обёртки, без преамбулы. ПЕРВЫЙ СИМВОЛ ОТВЕТА ДОЛЖЕН БЫТЬ "{":

{
  "id": "<slug lowercase underscore>",
  "title": "<human-readable map title>",
  "motivation": "<2-4 sentences: why this functionality exists>",
  "details": "<3-6 sentences: how it's structured, main patterns>",
  "clusters": [
    {
      "id": "<slug>",
      "title": "<Cluster Title>",
      "description": "<1 sentence>",
      "color": "<hex color, different for each cluster>",
      "steps": [
        {
          "id": "1a",
          "title": "<step title>",
          "filePath": "<must be one of the provided files>",
          "startLine": <int>,
          "endLine": <int>,
          "connections": ["1b"],
          "connectionLabels": ["initializes"]
        }
      ]
    }
  ]
}

КРИТИЧЕСКИЕ ТРЕБОВАНИЯ:
- filePath ОБЯЗАТЕЛЬНО должен быть одним из путей, присутствующих в переданном контексте. НЕ придумывай файлы.
- startLine/endLine ДОЛЖНЫ соответствовать реальным строкам в содержимом файла (нумерация с 1).
- id шагов: формат "<номер кластера><буква>" — "1a", "1b", "2a"... где номер = порядковый номер кластера.
- connections должны указывать на id шагов, которые РЕАЛЬНО присутствуют в clusters.
- 3-6 кластеров, каждый кластер 2-8 шагов.
- НЕ оборачивай JSON в markdown code fence.
- Если контекста недостаточно — верни JSON с пустым clusters: [] и заполни details пояснением.)";
}

QString GenerateSemanticMapTool::buildUserPrompt(const QJsonObject& args) const
{
    Q_UNUSED(args)
    return "Analyze the code above and produce the semantic map JSON.";
}

void GenerateSemanticMapTool::sendGenerationRequest(const QString& systemPrompt, const QString& userPrompt)
{
    if (!m_innerClient) {
        m_innerClient = new LLMClient(this);
        connect(m_innerClient, &LLMClient::streamToken, this, &GenerateSemanticMapTool::onMapStreamToken);
        connect(m_innerClient, &LLMClient::streamFinished, this, &GenerateSemanticMapTool::onMapStreamFinished);
        connect(m_innerClient, &LLMClient::errorOccurred, this, &GenerateSemanticMapTool::onMapErrorOccurred);
    }

    m_streamBuffer.clear();

    QJsonArray messages;
    QJsonObject msg;
    msg["role"] = "user";
    msg["content"] = systemPrompt + "\n\n" + userPrompt;
    messages.append(msg);

    m_innerClient->sendMessage(messages, QJsonArray{} /* no tools */);
}

void GenerateSemanticMapTool::onMapStreamToken(const QString& token)
{
    m_streamBuffer += token;
}

void GenerateSemanticMapTool::onMapStreamFinished(const QString& stopReason)
{
    Q_UNUSED(stopReason)

    // Strip markdown code fences if model wrapped JSON
    QString cleaned = m_streamBuffer.trimmed();
    if (cleaned.startsWith("```json"))
        cleaned = cleaned.mid(7);
    else if (cleaned.startsWith("```"))
        cleaned = cleaned.mid(3);
    if (cleaned.endsWith("```"))
        cleaned = cleaned.chopped(3);
    cleaned = cleaned.trimmed();

    ValidationResult result = validateSemanticMapJson(cleaned);

    if (result.ok) {
        SemanticMapStore store(m_projectRoot);
        store.save(result.map);

        QJsonDocument doc(result.map.toJson());
        emit finished(doc.toJson(QJsonDocument::Compact), false);
        return;
    }

    if (m_retryCount < kMaxRetries) {
        m_retryCount++;
        QString retryPrompt = m_originalUserPrompt +
            "\n\nPREVIOUS ATTEMPT FAILED: " + result.errorReason +
            "\nPlease fix this specific issue and return corrected JSON. "
            "Remember: filePath must be exactly one of the provided files, "
            "and line numbers must be within the actual file length.";
        sendGenerationRequest(m_originalSystemPrompt, retryPrompt);
        return;
    }

    emit finished("Error: failed to generate valid semantic map after " +
                   QString::number(kMaxRetries) + " retries. Last error: " +
                   result.errorReason, true);
}

void GenerateSemanticMapTool::onMapErrorOccurred(const QString& errorMessage)
{
    emit finished("Error: " + errorMessage, true);
}

ValidationResult GenerateSemanticMapTool::validateSemanticMapJson(const QString& rawJson) const
{
    ValidationResult vr;

    // Step 1: Parse JSON
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(rawJson.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        vr.errorReason = "Invalid JSON: " + parseError.errorString();
        return vr;
    }

    QJsonObject obj = doc.object();

    // Step 2: Required top-level fields
    if (!obj.contains("id") || !obj.contains("title") || !obj.contains("clusters")) {
        vr.errorReason = "Missing required top-level fields (id, title, or clusters)";
        return vr;
    }

    SemanticMap map = SemanticMap::fromJson(obj);

    // Empty clusters with details = valid "insufficient context" response
    if (map.clusters.isEmpty()) {
        vr.ok = true;
        vr.map = map;
        return vr;
    }

    // Step 3-4: Validate each step
    for (auto& cluster : map.clusters) {
        if (cluster.id.isEmpty() || cluster.title.isEmpty()) {
            vr.errorReason = "Cluster missing id or title";
            return vr;
        }

        for (auto& step : cluster.steps) {
            if (step.id.isEmpty()) {
                vr.errorReason = "Step missing id in cluster '" + cluster.id + "'";
                return vr;
            }

            // 4a: filePath must exist in available files (skip if task-only mode)
            if (!m_availableFiles.isEmpty() && !m_availableFiles.contains(step.filePath)) {
                vr.errorReason = "Step '" + step.id + "' references unknown file: " + step.filePath;
                return vr;
            }

            // 4b: Line range validity
            if (step.startLine < 1 || step.endLine < step.startLine) {
                vr.errorReason = "Step '" + step.id + "' has invalid line range: " +
                                 QString::number(step.startLine) + "-" + QString::number(step.endLine);
                return vr;
            }

            // 4c: endLine within file length
            QString fullPath = m_projectRoot + "/" + step.filePath;
            int numLines = countFileLines(fullPath);
            if (numLines > 0 && step.endLine > numLines) {
                vr.errorReason = "Step '" + step.id + "' line range " +
                                 QString::number(step.startLine) + "-" + QString::number(step.endLine) +
                                 " exceeds file length (" + QString::number(numLines) + ") in " +
                                 step.filePath;
                return vr;
            }

            // 5: Clean up dangling connections
            QStringList validIds;
            for (const auto& c : map.clusters)
                for (const auto& s : c.steps)
                    validIds.append(s.id);

            for (int i = step.connections.size() - 1; i >= 0; --i) {
                if (!validIds.contains(step.connections[i])) {
                    step.connections.removeAt(i);
                    if (i < step.connectionLabels.size())
                        step.connectionLabels.removeAt(i);
                }
            }

            // 6: Fill codeSnippet from real file
            step.codeSnippet = extractCodeSnippet(m_projectRoot, step.filePath,
                                                   step.startLine, step.endLine, 2);
        }
    }

    vr.ok = true;
    vr.map = map;
    return vr;
}

int GenerateSemanticMapTool::countFileLines(const QString& fullPath) const
{
    QFile file(fullPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return 0;

    int count = 0;
    QTextStream in(&file);
    while (!in.atEnd()) {
        in.readLine();
        count++;
    }
    return count;
}
