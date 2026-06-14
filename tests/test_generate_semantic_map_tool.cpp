#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "generate_semantic_map_tool.h"

class TestGenerateSemanticMapTool : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;
    GenerateSemanticMapTool* m_tool = nullptr;

    bool createFile(const QString& relPath, const QString& content)
    {
        QString fullPath = m_tempDir.path() + "/" + relPath;
        QDir().mkpath(QFileInfo(fullPath).absolutePath());
        QFile f(fullPath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;
        f.write(content.toUtf8());
        f.close();
        return true;
    }

    ValidationResult validate(const QString& rawJson)
    {
        return m_tool->validateSemanticMapJson(rawJson);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_tempDir.isValid());
        createFile("src/main.cpp",
            "#include <iostream>\n"
            "int main() {\n"
            "    return 0;\n"
            "}\n"
            "// end\n");

        m_tool = new GenerateSemanticMapTool(m_tempDir.path(), nullptr, this);
        m_tool->m_availableFiles = {"src/main.cpp"};
        m_tool->m_projectRoot = m_tempDir.path();
    }

    void cleanupTestCase()
    {
        delete m_tool;
        m_tool = nullptr;
    }

    void testValidJsonPasses()
    {
        QString json = R"({
            "id": "test_map",
            "title": "Test Map",
            "motivation": "Testing",
            "details": "Details here",
            "clusters": [{
                "id": "c1",
                "title": "Cluster 1",
                "description": "Desc",
                "color": "#2d3a4f",
                "steps": [{
                    "id": "1a",
                    "title": "Main function",
                    "filePath": "src/main.cpp",
                    "startLine": 1,
                    "endLine": 3,
                    "connections": [],
                    "connectionLabels": []
                }]
            }]
        })";

        ValidationResult result = validate(json);
        QVERIFY(result.ok);
        QCOMPARE(result.map.clusters.size(), 1);
        QCOMPARE(result.map.clusters[0].steps.size(), 1);
        QVERIFY(!result.map.clusters[0].steps[0].codeSnippet.isEmpty());
    }

    void testInvalidJsonSyntax()
    {
        ValidationResult result = validate("{ broken json");
        QVERIFY(!result.ok);
        QVERIFY(result.errorReason.contains("Invalid JSON"));
    }

    void testUnknownFilePath()
    {
        QString json = R"({
            "id": "test",
            "title": "Test",
            "clusters": [{
                "id": "c1",
                "title": "C1",
                "steps": [{
                    "id": "1a",
                    "title": "S1",
                    "filePath": "src/nonexistent.cpp",
                    "startLine": 1,
                    "endLine": 3,
                    "connections": [],
                    "connectionLabels": []
                }]
            }]
        })";

        ValidationResult result = validate(json);
        QVERIFY(!result.ok);
        QVERIFY(result.errorReason.contains("nonexistent.cpp"));
        QVERIFY(result.errorReason.contains("1a"));
    }

    void testLineNumberOutOfBounds()
    {
        QString json = R"({
            "id": "test",
            "title": "Test",
            "clusters": [{
                "id": "c1",
                "title": "C1",
                "steps": [{
                    "id": "1a",
                    "title": "S1",
                    "filePath": "src/main.cpp",
                    "startLine": 1,
                    "endLine": 100,
                    "connections": [],
                    "connectionLabels": []
                }]
            }]
        })";

        ValidationResult result = validate(json);
        QVERIFY(!result.ok);
        QVERIFY(result.errorReason.contains("100"));
    }

    void testStartLineGreaterThanEndLine()
    {
        QString json = R"({
            "id": "test",
            "title": "Test",
            "clusters": [{
                "id": "c1",
                "title": "C1",
                "steps": [{
                    "id": "1a",
                    "title": "S1",
                    "filePath": "src/main.cpp",
                    "startLine": 10,
                    "endLine": 5,
                    "connections": [],
                    "connectionLabels": []
                }]
            }]
        })";

        ValidationResult result = validate(json);
        QVERIFY(!result.ok);
        QVERIFY(result.errorReason.contains("invalid line range"));
    }

    void testDanglingConnectionRemoved()
    {
        QString json = R"({
            "id": "test",
            "title": "Test",
            "clusters": [{
                "id": "c1",
                "title": "C1",
                "steps": [{
                    "id": "1a",
                    "title": "S1",
                    "filePath": "src/main.cpp",
                    "startLine": 1,
                    "endLine": 3,
                    "connections": ["1b", "nonexistent_id"],
                    "connectionLabels": ["calls", "invalid"]
                }, {
                    "id": "1b",
                    "title": "S2",
                    "filePath": "src/main.cpp",
                    "startLine": 4,
                    "endLine": 5,
                    "connections": [],
                    "connectionLabels": []
                }]
            }]
        })";

        ValidationResult result = validate(json);
        QVERIFY(result.ok);

        // "1a" connections: "1b" kept, "nonexistent_id" removed
        QCOMPARE(result.map.clusters[0].steps[0].connections.size(), 1);
        QCOMPARE(result.map.clusters[0].steps[0].connections[0], QString("1b"));
        QCOMPARE(result.map.clusters[0].steps[0].connectionLabels.size(), 1);
        QCOMPARE(result.map.clusters[0].steps[0].connectionLabels[0], QString("calls"));
    }

    void testEmptyClustersWithDetailsIsValid()
    {
        QString json = R"({
            "id": "empty",
            "title": "Empty Map",
            "details": "Not enough context to analyze",
            "clusters": []
        })";

        ValidationResult result = validate(json);
        QVERIFY(result.ok);
        QVERIFY(result.map.clusters.isEmpty());
    }

    void testMarkdownWrappedJsonRejectedOrStripped()
    {
        // Current behavior: markdown-wrapped JSON fails validation
        // because onMapStreamFinished strips ```json before calling
        // validateSemanticMapJson, but the raw validator itself does NOT strip.
        // This test documents current behavior: raw ```json → Invalid JSON.
        // TODO: If LLM frequently wraps in ```json, the stripping in
        // onMapStreamFinished should be moved to validateSemanticMapJson.
        QString json = "```json\n{\"id\": \"x\", \"title\": \"x\", \"clusters\": []}\n```";

        ValidationResult result = validate(json);
        // Current behavior: fails because ```json is not valid JSON
        QVERIFY(!result.ok);
        QVERIFY(result.errorReason.contains("Invalid JSON"));
    }
};

QTEST_MAIN(TestGenerateSemanticMapTool)
#include "test_generate_semantic_map_tool.moc"
