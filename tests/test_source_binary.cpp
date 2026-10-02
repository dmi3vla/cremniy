#include <QtTest>
#include "SourceBinary/source_binary_store.h"

class TestSourceBinary : public QObject
{
    Q_OBJECT
private slots:
    void repeatedLineAndSectionCollision()
    {
        SourceBinaryStore store;
        ObjectFileIndex index;
        index.objectFilePath = "first.o";
        index.buildId = "build-A";
        SourceLineMapping first;
        first.filePath = "demo.cpp";
        first.lineNumber = 12;
        first.buildId = "build-A";
        first.sectionName = ".text.alpha";
        first.vaddr = 0x10;
        first.vaddrEnd = 0x13;
        SourceLineMapping second = first;
        second.vaddr = 0x20;
        second.vaddrEnd = 0x24;
        SourceLineMapping collision = first;
        collision.sectionName = ".text.beta";
        collision.lineNumber = 33;
        for (const auto& mapping : {first, second, collision}) {
            index.bySourceLine[mapping.filePath + ':' + QString::number(mapping.lineNumber)].append(mapping);
            index.byVaddr[sourceAddressKey(mapping.sectionName, mapping.vaddr)].append(mapping);
            DisasmInstruction instruction;
            instruction.address = QString::number(mapping.vaddr, 16);
            index.instructionsBySection[mapping.sectionName].append(instruction);
        }
        store.addIndex(index);

        const auto repeated = store.findAllBySourceLine("demo.cpp", 12, "build-A");
        QCOMPARE(repeated.size(), 2);
        QCOMPARE(repeated.at(0).vaddr, quint64(0x10));
        QCOMPARE(repeated.at(1).vaddr, quint64(0x20));
        QVERIFY(!store.findBySourceLine("demo.cpp", 12).has_value());
        QCOMPARE(store.findAllByVaddr(".text.alpha", 0x10, "build-A").size(), 1);
        QCOMPARE(store.findAllByVaddr(".text.beta", 0x10, "build-A").size(), 1);
        QCOMPARE(store.findAllByVaddr(0x10, "build-A").size(), 2);
        QVERIFY(!store.findByVaddr(0x10).has_value());
        QCOMPARE(store.instructionsInRange("build-A", ".text.alpha", 0x10, 0x11).size(), 1);
        QCOMPARE(store.instructionsInRange("build-A", ".text.beta", 0x10, 0x11).size(), 1);
    }

    void buildIsolation()
    {
        SourceBinaryStore store;
        for (const QString& id : {QStringLiteral("O0"), QStringLiteral("O2")}) {
            ObjectFileIndex index;
            index.objectFilePath = id + ".o";
            index.buildId = id;
            SourceLineMapping mapping;
            mapping.filePath = "demo.cpp";
            mapping.lineNumber = 9;
            mapping.sectionName = ".text";
            mapping.buildId = id;
            mapping.vaddr = id == "O0" ? 0x100 : 0x200;
            index.bySourceLine["demo.cpp:9"].append(mapping);
            index.byVaddr[sourceAddressKey(mapping.sectionName, mapping.vaddr)].append(mapping);
            DisasmInstruction instruction;
            instruction.address = QString::number(mapping.vaddr, 16);
            index.instructionsBySection[".text"].append(instruction);
            store.addIndex(index);
        }
        QCOMPARE(store.findAllBySourceLine("demo.cpp", 9, "O0").size(), 1);
        QCOMPARE(store.findAllBySourceLine("demo.cpp", 9, "O0").first().vaddr, quint64(0x100));
        QCOMPARE(store.findAllByVaddr(".text", 0x200, "O0").size(), 0);
        QCOMPARE(store.findAllByVaddr(".text", 0x200, "O2").size(), 1);
        QCOMPARE(store.instructionsInRange("O0", ".text", 0x100, 0x300).size(), 1);
        QCOMPARE(store.instructionsInRange("O2", ".text", 0x100, 0x300).size(), 1);
        QCOMPARE(store.instructionsInRange("O0", ".text", 0x100, 0x300).first().address, QString("100"));
        QCOMPARE(store.instructionsInRange("O2", ".text", 0x100, 0x300).first().address, QString("200"));
        QCOMPARE(store.instructionsInRange("O2", ".other", 0x100, 0x300).size(), 0);
    }
};

QTEST_MAIN(TestSourceBinary)
#include "test_source_binary.moc"
