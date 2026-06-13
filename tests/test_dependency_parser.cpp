#include <QtTest>
#include <QSignalSpy>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include "dependency_parser.h"

class TestDependencyParser : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

    bool createFile(const QString& relativePath, const QString& content)
    {
        QString fullPath = m_tempDir.path() + "/" + relativePath;
        QDir().mkpath(QFileInfo(fullPath).absolutePath());
        QFile file(fullPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;
        file.write(content.toUtf8());
        file.close();
        return true;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_tempDir.isValid());

        createFile("src/main.cpp",
            "#include \"utils/helper.h\"\n"
            "#include <iostream>\n"
            "int main() { return 0; }\n");

        createFile("src/utils/helper.h",
            "#pragma once\n"
            "#include \"math/add.h\"\n"
            "void helper();\n");

        createFile("src/math/add.h",
            "#pragma once\n"
            "int add(int a, int b);\n");

        createFile("src/standalone.cpp",
            "#include <vector>\n"
            "void foo() {}\n");
    }

    void testParserFindsAllFiles()
    {
        DependencyParser parser(m_tempDir.path());
        QSignalSpy spy(&parser, &DependencyParser::graphReady);
        parser.startParsing();

        QVERIFY(spy.wait(5000));
        QCOMPARE(spy.count(), 1);

        DependencyGraph graph = spy.at(0).at(0).value<DependencyGraph>();
        QCOMPARE(graph.allFiles.size(), 4);
    }

    void testParserFindsIncludes()
    {
        DependencyParser parser(m_tempDir.path());
        QSignalSpy spy(&parser, &DependencyParser::graphReady);
        parser.startParsing();

        QVERIFY(spy.wait(5000));

        DependencyGraph graph = spy.at(0).at(0).value<DependencyGraph>();
        QVERIFY(!graph.includes.empty());

        QString mainPath = m_tempDir.path() + "/src/main.cpp";
        QVERIFY(graph.includes.find(mainPath) != graph.includes.end());
        QCOMPARE(graph.includes.at(mainPath).size(), 1);
    }

    void testParserResolvesRelativePaths()
    {
        DependencyParser parser(m_tempDir.path());
        QSignalSpy spy(&parser, &DependencyParser::graphReady);
        parser.startParsing();

        QVERIFY(spy.wait(5000));

        DependencyGraph graph = spy.at(0).at(0).value<DependencyGraph>();

        QString helperPath = m_tempDir.path() + "/src/utils/helper.h";
        QVERIFY(graph.includes.find(helperPath) != graph.includes.end());

        QString addPath = m_tempDir.path() + "/src/math/add.h";
        QVERIFY(graph.includes.at(helperPath).contains(addPath));
    }

    void testNoDuplicates()
    {
        DependencyParser parser(m_tempDir.path());
        QSignalSpy spy(&parser, &DependencyParser::graphReady);
        parser.startParsing();

        QVERIFY(spy.wait(5000));

        DependencyGraph graph = spy.at(0).at(0).value<DependencyGraph>();

        QSet<QString> uniqueFiles;
        for (const QString& file : graph.allFiles)
            uniqueFiles.insert(file);
        QCOMPARE(uniqueFiles.size(), graph.allFiles.size());
    }

    void testEmptyProject()
    {
        QTemporaryDir emptyDir;
        QVERIFY(emptyDir.isValid());

        DependencyParser parser(emptyDir.path());
        QSignalSpy spy(&parser, &DependencyParser::graphReady);
        parser.startParsing();

        QVERIFY(spy.wait(5000));

        DependencyGraph graph = spy.at(0).at(0).value<DependencyGraph>();
        QVERIFY(graph.allFiles.isEmpty());
        QVERIFY(graph.includes.empty());
    }

    void testGraphUpdatedSignal()
    {
        DependencyParser parser(m_tempDir.path());
        QSignalSpy readySpy(&parser, &DependencyParser::graphReady);
        QSignalSpy updatedSpy(&parser, &DependencyParser::graphUpdated);

        parser.startParsing();
        QVERIFY(readySpy.wait(5000));
        QCOMPARE(readySpy.count(), 1);
        QCOMPARE(updatedSpy.count(), 0);

        parser.startParsing();
        QVERIFY(updatedSpy.wait(5000));
        QCOMPARE(updatedSpy.count(), 1);
    }
};

QTEST_MAIN(TestDependencyParser)
#include "test_dependency_parser.moc"
