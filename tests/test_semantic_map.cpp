#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include "semantic_map.h"
#include "semantic_map_store.h"
#include "semantic_map_utils.h"

class TestSemanticMap : public QObject
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

    void testStepRoundTrip()
    {
        SemanticStep step;
        step.id = "1a";
        step.title = "QApplication creation";
        step.filePath = "src/main.cpp";
        step.startLine = 5;
        step.endLine = 12;
        step.codeSnippet = "int main() { ... }";
        step.connections = {"1b", "2a"};
        step.connectionLabels = {"initializes", "calls"};

        QJsonObject obj = step.toJson();
        SemanticStep restored = SemanticStep::fromJson(obj);

        QCOMPARE(restored.id, step.id);
        QCOMPARE(restored.title, step.title);
        QCOMPARE(restored.filePath, step.filePath);
        QCOMPARE(restored.startLine, step.startLine);
        QCOMPARE(restored.endLine, step.endLine);
        QCOMPARE(restored.codeSnippet, step.codeSnippet);
        QCOMPARE(restored.connections, step.connections);
        QCOMPARE(restored.connectionLabels, step.connectionLabels);
    }

    void testClusterRoundTrip()
    {
        SemanticCluster cluster;
        cluster.id = "startup";
        cluster.title = "Application Startup";
        cluster.description = "Init sequence";
        cluster.color = "#2d3a4f";

        SemanticStep s1;
        s1.id = "1a";
        s1.title = "Step 1";
        s1.filePath = "src/a.cpp";
        s1.startLine = 1;
        s1.endLine = 5;

        SemanticStep s2;
        s2.id = "1b";
        s2.title = "Step 2";
        s2.filePath = "src/b.cpp";
        s2.startLine = 10;
        s2.endLine = 20;
        s2.connections = {"1a"};
        s2.connectionLabels = {"depends on"};

        cluster.steps = {s1, s2};

        QJsonObject obj = cluster.toJson();
        SemanticCluster restored = SemanticCluster::fromJson(obj);

        QCOMPARE(restored.id, cluster.id);
        QCOMPARE(restored.title, cluster.title);
        QCOMPARE(restored.description, cluster.description);
        QCOMPARE(restored.color, cluster.color);
        QCOMPARE(restored.steps.size(), 2);
        QCOMPARE(restored.steps[0].id, s1.id);
        QCOMPARE(restored.steps[1].connections.size(), 1);
        QCOMPARE(restored.steps[1].connections[0], QString("1a"));
    }

    void testMapRoundTrip()
    {
        SemanticMap map;
        map.id = "test_map";
        map.title = "Test Map";
        map.motivation = "Testing serialization";
        map.details = "Some details here";
        map.createdAt = QDateTime(QDate(2025, 6, 14), QTime(12, 30, 0));

        SemanticCluster c1;
        c1.id = "c1";
        c1.title = "Cluster 1";
        SemanticStep s1;
        s1.id = "1a";
        s1.title = "S1";
        c1.steps = {s1};

        SemanticCluster c2;
        c2.id = "c2";
        c2.title = "Cluster 2";

        map.clusters = {c1, c2};

        QJsonObject obj = map.toJson();
        SemanticMap restored = SemanticMap::fromJson(obj);

        QCOMPARE(restored.id, map.id);
        QCOMPARE(restored.title, map.title);
        QCOMPARE(restored.motivation, map.motivation);
        QCOMPARE(restored.details, map.details);
        QCOMPARE(restored.createdAt.toString(Qt::ISODate), map.createdAt.toString(Qt::ISODate));
        QCOMPARE(restored.clusters.size(), 2);
        QCOMPARE(restored.clusters[0].id, c1.id);
    }

    void testFromJsonMissingOptionalFields()
    {
        QJsonObject obj;
        obj["id"] = "minimal";
        obj["title"] = "Minimal Map";

        SemanticMap map = SemanticMap::fromJson(obj);
        QCOMPARE(map.id, QString("minimal"));
        QCOMPARE(map.title, QString("Minimal Map"));
        QCOMPARE(map.motivation, QString());
        QCOMPARE(map.details, QString());
        QVERIFY(map.clusters.isEmpty());

        QJsonObject clusterObj;
        clusterObj["id"] = "c1";
        clusterObj["title"] = "C1";
        SemanticCluster cluster = SemanticCluster::fromJson(clusterObj);
        QCOMPARE(cluster.description, QString());
        QCOMPARE(cluster.color, QString());
        QVERIFY(cluster.steps.isEmpty());

        QJsonObject stepObj;
        stepObj["id"] = "s1";
        stepObj["title"] = "S1";
        SemanticStep step = SemanticStep::fromJson(stepObj);
        QCOMPARE(step.filePath, QString());
        QCOMPARE(step.startLine, 0);
        QVERIFY(step.connections.isEmpty());
        QVERIFY(step.connectionLabels.isEmpty());
    }

    void testFindStep()
    {
        SemanticMap map;
        map.id = "test";

        SemanticCluster c1;
        c1.id = "c1";
        SemanticStep s1;
        s1.id = "1a";
        s1.title = "Step 1a";
        SemanticStep s2;
        s2.id = "1b";
        s2.title = "Step 1b";
        c1.steps = {s1, s2};

        SemanticCluster c2;
        c2.id = "c2";
        SemanticStep s3;
        s3.id = "2a";
        s3.title = "Step 2a";
        c2.steps = {s3};

        map.clusters = {c1, c2};

        const SemanticStep* found = map.findStep("1b");
        QVERIFY(found != nullptr);
        QCOMPARE(found->title, QString("Step 1b"));

        const SemanticStep* found2 = map.findStep("2a");
        QVERIFY(found2 != nullptr);
        QCOMPARE(found2->title, QString("Step 2a"));

        const SemanticStep* notFound = map.findStep("nonexistent");
        QVERIFY(notFound == nullptr);
    }

    // --- extractCodeSnippet ---

    void testExtractCodeSnippetValidRange()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        createFile(dir, "file.txt", "line1\nline2\nline3\nline4\nline5\n");

        QString snippet = extractCodeSnippet(dir.path(), "file.txt", 2, 4);
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
        createFile(dir, "file.txt", "line1\nline2\nline3\nline4\nline5\n");

        QString snippet = extractCodeSnippet(dir.path(), "file.txt", 10, 20);
        QVERIFY(snippet.isEmpty());
    }

    void testExtractCodeSnippetInvalidRange()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        createFile(dir, "file.txt", "line1\nline2\nline3\n");

        // startLine > endLine
        QVERIFY(extractCodeSnippet(dir.path(), "file.txt", 5, 2).isEmpty());

        // startLine < 1
        QVERIFY(extractCodeSnippet(dir.path(), "file.txt", 0, 3).isEmpty());
        QVERIFY(extractCodeSnippet(dir.path(), "file.txt", -1, 3).isEmpty());
    }

    void testExtractCodeSnippetMissingFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        QString snippet = extractCodeSnippet(dir.path(), "nonexistent.cpp", 1, 5);
        QVERIFY(snippet.isEmpty());
    }

    // --- SemanticMapStore ---

    void testStoreSaveAndLoad()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        SemanticMapStore store(dir.path());

        SemanticMap map;
        map.id = "test_map_1";
        map.title = "Test Map";
        map.motivation = "Testing";
        map.createdAt = QDateTime::currentDateTime();

        SemanticCluster c;
        c.id = "c1";
        c.title = "Cluster 1";
        SemanticStep s;
        s.id = "1a";
        s.title = "Step 1";
        s.filePath = "src/main.cpp";
        s.startLine = 1;
        s.endLine = 5;
        c.steps = {s};
        map.clusters = {c};

        QVERIFY(store.save(map));

        auto loaded = store.load("test_map_1");
        QVERIFY(loaded.has_value());
        QCOMPARE(loaded->id, map.id);
        QCOMPARE(loaded->title, map.title);
        QCOMPARE(loaded->motivation, map.motivation);
        QCOMPARE(loaded->clusters.size(), 1);
        QCOMPARE(loaded->clusters[0].steps[0].filePath, s.filePath);
    }

    void testStoreLoadNonexistent()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        SemanticMapStore store(dir.path());
        auto result = store.load("nonexistent_id");
        QVERIFY(!result.has_value());
    }

    void testStoreList()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        SemanticMapStore store(dir.path());

        SemanticMap map1;
        map1.id = "map_alpha";
        map1.title = "Alpha Map";
        map1.createdAt = QDateTime(QDate(2025, 1, 1), QTime(10, 0, 0));

        SemanticMap map2;
        map2.id = "map_beta";
        map2.title = "Beta Map";
        map2.createdAt = QDateTime(QDate(2025, 6, 1), QTime(12, 0, 0));

        store.save(map1);
        store.save(map2);

        auto list = store.list();
        QCOMPARE(list.size(), 2);

        bool foundAlpha = false, foundBeta = false;
        for (const auto& m : list) {
            if (m.id == "map_alpha") {
                foundAlpha = true;
                QCOMPARE(m.title, QString("Alpha Map"));
            }
            if (m.id == "map_beta") {
                foundBeta = true;
                QCOMPARE(m.title, QString("Beta Map"));
            }
        }
        QVERIFY(foundAlpha);
        QVERIFY(foundBeta);
    }

    void testStoreSanitizeId()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        SemanticMapStore store(dir.path());

        SemanticMap map;
        map.id = "../../etc/passwd";
        map.title = "Evil Map";
        map.createdAt = QDateTime::currentDateTime();

        QVERIFY(store.save(map));

        // File should be at sanitized path, NOT outside storage dir
        QString expectedPath = dir.path() + "/.cremniy/semantic_maps/______etc_passwd.json";
        QVERIFY(QFile::exists(expectedPath));

        // Verify no file was created at traversal path
        QVERIFY(!QFile::exists(dir.path() + "/../../etc/passwd.json"));
    }

    void testStoreRemove()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        SemanticMapStore store(dir.path());

        SemanticMap map;
        map.id = "removable";
        map.title = "Will be removed";
        map.createdAt = QDateTime::currentDateTime();

        store.save(map);
        QVERIFY(store.load("removable").has_value());

        QVERIFY(store.remove("removable"));
        QVERIFY(!store.load("removable").has_value());

        auto list = store.list();
        for (const auto& m : list)
            QVERIFY(m.id != "removable");
    }
};

QTEST_MAIN(TestSemanticMap)
#include "test_semantic_map.moc"
