#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "generate_codemap_tool.h"

class TestGenerateCodemapTool : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;
    GenerateCodemapTool* m_tool = nullptr;

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

    CodemapValidationResult validate(const QString& rawJson)
    {
        return m_tool->validateCodemapJson(rawJson);
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

        m_tool = new GenerateCodemapTool(m_tempDir.path(), nullptr, this);
        m_tool->m_availableFiles = {"src/main.cpp"};
        m_tool->m_projectRoot = m_tempDir.path();
    }

    void cleanupTestCase()
    {
        delete m_tool;
        m_tool = nullptr;
    }

    void testValidCodemapPasses()
    {
        QString json =
            "{\"schemaVersion\": 1,"
            "\"id\": \"test-codemap\","
            "\"stableId\": \"test-uuid\","
            "\"metadata\": {\"generationSource\": \"test\"},"
            "\"title\": \"Test Codemap\","
            "\"traces\": [{"
            "\"id\": \"1\","
            "\"title\": \"Main\","
            "\"description\": \"Entry\","
            "\"locations\": [{"
            "\"id\": \"1a\","
            "\"path\": \"src/main.cpp\","
            "\"lineNumber\": 2,"
            "\"lineContent\": \"int main() {\","
            "\"title\": \"Main function\","
            "\"description\": \"Entry point\""
            "}],"
            "\"traceTextDiagram\": \"main()\","
            "\"traceGuide\": \"# Motivation\\nTest\\n\\n# Details\\nDetails\","
            "\"color\": \"#2d3a4f\""
            "}],"
            "\"mermaidDiagram\": \"\""
            "}";

        CodemapValidationResult result = validate(json);
        QVERIFY(result.ok);
        QCOMPARE(result.map.traces.size(), 1);
        QCOMPARE(result.map.traces[0].locations.size(), 1);
    }

    void testInvalidJsonSyntax()
    {
        CodemapValidationResult result = validate("{ broken json");
        QVERIFY(!result.ok);
        QVERIFY(result.errorReason.contains("Invalid JSON"));
    }

    void testUnknownFilePath()
    {
        QString json =
            "{\"title\": \"Test\","
            "\"traces\": [{"
            "\"id\": \"1\", \"title\": \"T\","
            "\"locations\": [{"
            "\"id\": \"1a\", \"path\": \"src/nonexistent.cpp\","
            "\"lineNumber\": 1, \"lineContent\": \"x\","
            "\"title\": \"S\""
            "}]"
            "}]"
            "}";

        CodemapValidationResult result = validate(json);
        QVERIFY(!result.ok);
        QVERIFY(result.errorReason.contains("nonexistent.cpp"));
    }

    void testLineNumberOutOfBounds()
    {
        QString json =
            "{\"title\": \"Test\","
            "\"traces\": [{"
            "\"id\": \"1\", \"title\": \"T\","
            "\"locations\": [{"
            "\"id\": \"1a\", \"path\": \"src/main.cpp\","
            "\"lineNumber\": 100, \"lineContent\": \"x\","
            "\"title\": \"S\""
            "}]"
            "}]"
            "}";

        CodemapValidationResult result = validate(json);
        QVERIFY(!result.ok);
        QVERIFY(result.errorReason.contains("100"));
    }

    void testLineNumberZero()
    {
        QString json =
            "{\"title\": \"Test\","
            "\"traces\": [{"
            "\"id\": \"1\", \"title\": \"T\","
            "\"locations\": [{"
            "\"id\": \"1a\", \"path\": \"src/main.cpp\","
            "\"lineNumber\": 0, \"lineContent\": \"\","
            "\"title\": \"S\""
            "}]"
            "}]"
            "}";

        CodemapValidationResult result = validate(json);
        QVERIFY(!result.ok);
        QVERIFY(result.errorReason.contains("invalid lineNumber"));
    }

    void testLineContentMismatch()
    {
        QString json =
            "{\"title\": \"Test\","
            "\"traces\": [{"
            "\"id\": \"1\", \"title\": \"T\","
            "\"locations\": [{"
            "\"id\": \"1a\", \"path\": \"src/main.cpp\","
            "\"lineNumber\": 2,"
            "\"lineContent\": \"WRONG CONTENT\","
            "\"title\": \"S\""
            "}]"
            "}]"
            "}";

        CodemapValidationResult result = validate(json);
        QVERIFY(!result.ok);
        QVERIFY(result.errorReason.contains("lineContent mismatch"));
    }

    void testEmptyTracesWithDetailsIsValid()
    {
        QString json =
            "{\"title\": \"Empty\","
            "\"metadata\": {\"originalPrompt\": \"Not enough context\"},"
            "\"traces\": []"
            "}";

        CodemapValidationResult result = validate(json);
        QVERIFY(result.ok);
        QVERIFY(result.map.traces.isEmpty());
    }

    void testMarkdownWrappedJson()
    {
        QString json = "```json\n{\"title\": \"x\", \"traces\": []}\n```";

        CodemapValidationResult result = validate(json);
        QVERIFY(!result.ok);
        QVERIFY(result.errorReason.contains("Invalid JSON"));
    }
};

QTEST_MAIN(TestGenerateCodemapTool)
#include "test_generate_codemap_tool.moc"
