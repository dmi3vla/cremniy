#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include "codemap.h"
#include "codemap_store.h"
#include "codemap_utils.h"

class TestCodemap : public QObject
{
    Q_OBJECT

private:
    bool createFile(const QTemporaryDir& dir, const QString& relPath, const QString& content)
    {
        QString fullPath = dir.path() + "/" + relPath;
        QDir().mkpath(QFileInfo(fullPath).absolutePath());
        QFile f(fullPath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;
        f.write(content.toUtf8());
        f.close();
        return true;
    }

private slots:
    // --- Serialization round-trip ---

    void testLocationRoundTrip()
    {
        CodemapLocation loc;
        loc.id = "1a";
        loc.path = "src/main.cpp";
        loc.lineNumber = 6;
        loc.lineContent = "int main() {";
        loc.title = "Main function";
        loc.description = "Entry point";

        QJsonObject obj = loc.toJson();
        CodemapLocation restored = CodemapLocation::fromJson(obj);

        QCOMPARE(restored.id, loc.id);
        QCOMPARE(restored.path, loc.path);
        QCOMPARE(restored.lineNumber, loc.lineNumber);
        QCOMPARE(restored.lineContent, loc.lineContent);
        QCOMPARE(restored.title, loc.title);
        QCOMPARE(restored.description, loc.description);
    }

    void testTraceRoundTrip()
    {
        CodemapTrace trace;
        trace.id = "1";
        trace.title = "Application Startup";
        trace.description = "Init sequence";
        trace.traceTextDiagram = "main()\n├── init()";
        trace.traceGuide = "# Motivation\nTest\n\n# Details\nDetails here";
        trace.color = "#2d3a4f";

        CodemapLocation l1;
        l1.id = "1a";
        l1.path = "src/main.cpp";
        l1.lineNumber = 6;
        l1.lineContent = "int main() {";
        l1.title = "Main";

        CodemapLocation l2;
        l2.id = "1b";
        l2.path = "src/init.cpp";
        l2.lineNumber = 10;
        l2.lineContent = "void init() {";
        l2.title = "Init";

        trace.locations = {l1, l2};

        QJsonObject obj = trace.toJson();
        CodemapTrace restored = CodemapTrace::fromJson(obj);

        QCOMPARE(restored.id, trace.id);
        QCOMPARE(restored.title, trace.title);
        QCOMPARE(restored.description, trace.description);
        QCOMPARE(restored.traceTextDiagram, trace.traceTextDiagram);
        QCOMPARE(restored.traceGuide, trace.traceGuide);
        QCOMPARE(restored.color, trace.color);
        QCOMPARE(restored.locations.size(), 2);
        QCOMPARE(restored.locations[0].id, l1.id);
        QCOMPARE(restored.locations[1].lineContent, l2.lineContent);
    }

    void testCodemapRoundTrip()
    {
        Codemap map;
        map.schemaVersion = 1;
        map.id = "test-codemap";
        map.stableId = "550e8400-e29b-41d4-a716-446655440000";
        map.title = "Test Codemap";
        map.mermaidDiagram = "graph TD\n  1a -->|init| 2a";
        map.metadata.generationSource = "test";
        map.metadata.generationTimestamp = "2025-06-14T12:00:00Z";

        CodemapTrace t1;
        t1.id = "1";
        t1.title = "Trace 1";
        CodemapLocation l1;
        l1.id = "1a";
        l1.path = "src/a.cpp";
        l1.lineNumber = 1;
        l1.lineContent = "line1";
        t1.locations = {l1};
        map.traces = {t1};

        QJsonObject obj = map.toJson();
        Codemap restored = Codemap::fromJson(obj);

        QCOMPARE(restored.schemaVersion, 1);
        QCOMPARE(restored.id, map.id);
        QCOMPARE(restored.stableId, map.stableId);
        QCOMPARE(restored.title, map.title);
        QCOMPARE(restored.mermaidDiagram, map.mermaidDiagram);
        QCOMPARE(restored.metadata.generationSource, QString("test"));
        QCOMPARE(restored.traces.size(), 1);
        QCOMPARE(restored.traces[0].locations[0].id, l1.id);
    }

    void testFromJsonMissingOptionalFields()
    {
        QJsonObject obj;
        obj["title"] = "Minimal";

        Codemap map = Codemap::fromJson(obj);
        QCOMPARE(map.title, QString("Minimal"));
        QVERIFY(map.traces.isEmpty());
        QVERIFY(map.mermaidDiagram.isEmpty());
        QVERIFY(!map.stableId.isEmpty()); // auto-generated UUID

        QJsonObject traceObj;
        traceObj["id"] = "1";
        traceObj["title"] = "T1";
        CodemapTrace trace = CodemapTrace::fromJson(traceObj);
        QVERIFY(trace.locations.isEmpty());
        QVERIFY(trace.traceGuide.isEmpty());
        QVERIFY(trace.color.isEmpty());
    }

    void testFindLocation()
    {
        Codemap map;
        CodemapTrace t1;
        t1.id = "1";
        CodemapLocation l1;
        l1.id = "1a";
        l1.title = "L1a";
        CodemapLocation l2;
        l2.id = "1b";
        l2.title = "L1b";
        t1.locations = {l1, l2};

        CodemapTrace t2;
        t2.id = "2";
        CodemapLocation l3;
        l3.id = "2a";
        l3.title = "L2a";
        t2.locations = {l3};

        map.traces = {t1, t2};

        const CodemapLocation* found = map.findLocation("1b");
        QVERIFY(found != nullptr);
        QCOMPARE(found->title, QString("L1b"));

        const CodemapLocation* found2 = map.findLocation("2a");
        QVERIFY(found2 != nullptr);

        const CodemapLocation* notFound = map.findLocation("nonexistent");
        QVERIFY(notFound == nullptr);
    }

    void testParsedConnections()
    {
        Codemap map;
        map.mermaidDiagram = "graph TD\n  1a -->|initializes| 2a\n  2b -->|calls| 3a";

        auto edges = map.parsedConnections();
        QCOMPARE(edges.size(), 2);
        QCOMPARE(edges[0].from, QString("1a"));
        QCOMPARE(edges[0].to, QString("2a"));
        QCOMPARE(edges[0].label, QString("initializes"));
        QCOMPARE(edges[1].from, QString("2b"));
        QCOMPARE(edges[1].to, QString("3a"));
        QCOMPARE(edges[1].label, QString("calls"));
    }

    void testParsedConnectionsNoLabel()
    {
        Codemap map;
        map.mermaidDiagram = "graph TD\n  1a --> 2a";

        auto edges = map.parsedConnections();
        QCOMPARE(edges.size(), 1);
        QCOMPARE(edges[0].from, QString("1a"));
        QCOMPARE(edges[0].to, QString("2a"));
        QVERIFY(edges[0].label.isEmpty());
    }

    void testMotivationDetailsParsing()
    {
        CodemapTrace trace;
        trace.traceGuide = "# Motivation\nThis is why it exists.\n\n# Details\nThis is how it works.\nMore details.";

        QCOMPARE(trace.motivationText(), QString("This is why it exists."));
        QVERIFY(trace.detailsText().contains("This is how it works."));
    }

    // --- extractCodeSnippet ---

    void testExtractCodeSnippetValidRange()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        createFile(dir, "file.txt", "line1\nline2\nline3\nline4\nline5\n");

        QString snippet = extractCodeSnippet(dir.path(), "file.txt", 3, 1);
        QStringList lines = snippet.split("\n");
        QCOMPARE(lines.size(), 3);
        QCOMPARE(lines[0], QString("line2"));
        QCOMPARE(lines[1], QString("line3"));
        QCOMPARE(lines[2], QString("line4"));
    }

    void testExtractCodeSnippetOutOfBounds()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        createFile(dir, "file.txt", "line1\nline2\nline3\n");

        QVERIFY(extractCodeSnippet(dir.path(), "file.txt", 100, 3).isEmpty());
    }

    void testExtractCodeSnippetInvalidLine()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        createFile(dir, "file.txt", "line1\n");

        QVERIFY(extractCodeSnippet(dir.path(), "file.txt", 0, 3).isEmpty());
        QVERIFY(extractCodeSnippet(dir.path(), "file.txt", -1, 3).isEmpty());
    }

    void testExtractCodeSnippetMissingFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(extractCodeSnippet(dir.path(), "nonexistent.cpp", 1, 3).isEmpty());
    }

    // --- checkLocationStale ---

    void testCheckLocationStaleWhenCurrent()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        createFile(dir, "file.cpp", "line1\nint main() {\n    return 0;\n}\n");

        CodemapLocation loc;
        loc.path = "file.cpp";
        loc.lineNumber = 2;
        loc.lineContent = "int main() {";

        QVERIFY(!checkLocationStale(dir.path(), loc));
    }

    void testCheckLocationStaleWhenChanged()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        createFile(dir, "file.cpp", "line1\nint main() {\n    return 0;\n}\n");

        CodemapLocation loc;
        loc.path = "file.cpp";
        loc.lineNumber = 2;
        loc.lineContent = "old content that doesn't match";

        QVERIFY(checkLocationStale(dir.path(), loc));
    }

    void testCheckLocationStaleMissingFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        CodemapLocation loc;
        loc.path = "nonexistent.cpp";
        loc.lineNumber = 1;
        loc.lineContent = "something";

        QVERIFY(checkLocationStale(dir.path(), loc));
    }

    // --- Path conversion ---

    void testToRelativePath()
    {
        QCOMPARE(toRelativePath("/home/user/project/src/main.cpp", "/home/user/project"),
                 QString("src/main.cpp"));
        QCOMPARE(toRelativePath("/other/path/file.cpp", "/home/user/project"),
                 QString("/other/path/file.cpp"));
    }

    void testToAbsolutePath()
    {
        QCOMPARE(toAbsolutePath("src/main.cpp", "/home/user/project"),
                 QString("/home/user/project/src/main.cpp"));
        QCOMPARE(toAbsolutePath("/absolute/path.cpp", "/home/user/project"),
                 QString("/absolute/path.cpp"));
    }

    void testNormalizeAbsolutePaths()
    {
        Codemap map;
        CodemapTrace trace;
        CodemapLocation loc;
        loc.path = "/home/user/project/src/main.cpp";
        trace.locations = {loc};
        map.traces = {trace};

        map.normalizeAbsolutePaths("/home/user/project");
        QCOMPARE(map.traces[0].locations[0].path, QString("src/main.cpp"));
    }

    // --- CodemapStore ---

    void testStoreSaveAndLoad()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        CodemapStore store(dir.path());

        Codemap map;
        map.id = "test_codemap";
        map.title = "Test";
        map.stableId = "test-uuid";
        map.metadata.generationTimestamp = QDateTime::currentDateTime().toString(Qt::ISODate);

        CodemapTrace trace;
        trace.id = "1";
        trace.title = "Trace 1";
        CodemapLocation loc;
        loc.id = "1a";
        loc.path = "src/main.cpp";
        loc.lineNumber = 6;
        loc.lineContent = "int main() {";
        trace.locations = {loc};
        map.traces = {trace};

        QVERIFY(store.save(map));

        auto loaded = store.loadLatest();
        QVERIFY(loaded.has_value());
        QCOMPARE(loaded->id, map.id);
        QCOMPARE(loaded->title, map.title);
        QCOMPARE(loaded->traces.size(), 1);
        QCOMPARE(loaded->traces[0].locations[0].path, loc.path);
    }

    void testStoreLoadNonexistent()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        CodemapStore store(dir.path());
        QVERIFY(!store.exists());
        QVERIFY(!store.load().has_value());
    }

    void testStoreDefaultFilePath()
    {
        CodemapStore store("/home/user/myproject");
        // defaultFilePath uses buildFileName with empty Codemap
        QVERIFY(store.defaultFilePath().contains("codemap"));
        QVERIFY(store.defaultFilePath().contains(".codemap.txt"));
    }

    void testSlugifyTitle()
    {
        QCOMPARE(CodemapStore::slugifyTitle("Qt Application Entry Point"),
                 QString("Qt_Application_Entry_Point"));
        QCOMPARE(CodemapStore::slugifyTitle("Hello World!"),
                 QString("Hello_World"));
        QCOMPARE(CodemapStore::slugifyTitle("  spaces  "),
                 QString("spaces"));
    }

    void testBuildFileName()
    {
        Codemap map;
        map.title = "Qt Application Entry Point";
        map.metadata.generationTimestamp = "2026-06-14T12:31:13Z";

        QString fileName = CodemapStore::buildFileName(map);
        QVERIFY(fileName.startsWith("Qt_Application_Entry_Point_"));
        QVERIFY(fileName.endsWith(".codemap.txt"));
        QVERIFY(fileName.contains("20260614"));
    }

    void testStoreSaveAndLoadLatest()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        CodemapStore store(dir.path());

        Codemap map;
        map.id = "test_codemap";
        map.title = "Test Codemap";
        map.metadata.generationTimestamp = QDateTime::currentDateTime().toString(Qt::ISODate);

        CodemapTrace trace;
        trace.id = "1";
        trace.title = "Trace 1";
        CodemapLocation loc;
        loc.id = "1a";
        loc.path = "src/main.cpp";
        loc.lineNumber = 6;
        loc.lineContent = "int main() {";
        trace.locations = {loc};
        map.traces = {trace};

        QVERIFY(store.save(map));

        auto list = store.list();
        QCOMPARE(list.size(), 1);
        QVERIFY(list[0].title == "Test Codemap");

        auto loaded = store.loadLatest();
        QVERIFY(loaded.has_value());
        QCOMPARE(loaded->id, map.id);
        QCOMPARE(loaded->traces.size(), 1);
        QCOMPARE(loaded->traces[0].locations[0].path, loc.path);
    }
};

QTEST_MAIN(TestCodemap)
#include "test_codemap.moc"
