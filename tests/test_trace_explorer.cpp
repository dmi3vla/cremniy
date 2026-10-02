#include <QtTest>
#include <QComboBox>
#include <QCheckBox>
#include <QFile>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QJsonDocument>
#include <QSpinBox>
#include <QTextBrowser>
#include <QTemporaryDir>
#include <QTreeWidget>
#include "ToolTabs/Canvas/trace_explorer.h"

class TestTraceExplorer : public QObject
{
    Q_OBJECT
private:
    QString manifest() const { return QStringLiteral(TRACE_FIXTURE_ROOT) + "/trace-manifest.json"; }

private slots:
    void modelEvidenceAndPersistence()
    {
        TraceModel model;
        QString error;
        QVERIFY2(model.load(manifest(), &error), qPrintable(error));
        QVERIFY2(model.evidenceErrors().isEmpty(), qPrintable(model.evidenceErrors().join(';')));
        const QString helper = "demo.cpp::shared_helper(int)";
        QCOMPARE(model.appearances(helper).size(), 2);
        QVERIFY(model.linksForSymbol(helper).size() >= 2);
        int calcSymbols = 0;
        for (const auto& value : model.linksForRequirement("REQ-CALC"))
            if (value.toObject().value("kind").toString() == "requirement-symbol") ++calcSymbols;
        QCOMPARE(calcSymbols, 4);
        QVERIFY(model.diffText().contains("+    return calculate_beta(input) + shared_helper(input);"));
        QVERIFY(model.range(helper).value("status").toString() == "mapped");
        const QString o0 = model.selectedBuildId();
        const auto first = model.range(helper).value("instructions").toArray().first().toString();
        const auto mappings = model.sourceBinary().findAllByVaddr(
            ".text", first.toULongLong(nullptr, 0), o0);
        QVERIFY(!mappings.isEmpty());
        QCOMPARE(mappings.first().revision, QString("head"));

        QString o2;
        for (const auto& value : model.document().value("builds").toArray()) {
            const auto build = value.toObject();
            if (build.value("revision") == "head" && build.value("optimization") == "O2")
                o2 = build.value("id").toString();
        }
        QVERIFY(model.selectBuild(o2));
        QCOMPARE(model.range("demo.cpp::debug_only(int)").value("status").toString(),
                 QString("optimized-away-or-unavailable"));
        for (const auto& mapping : model.sourceBinary().findAllByVaddr(
                 ".text", first.toULongLong(nullptr, 0), o2))
            QCOMPARE(mapping.buildId, o2);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString saved = dir.filePath("session.json");
        QVERIFY2(model.save(saved, &error), qPrintable(error));
        TraceModel reopened;
        QVERIFY2(reopened.load(saved, &error), qPrintable(error));
        QCOMPARE(reopened.selectedBuildId(), o2);
        QCOMPARE(reopened.linksForSymbol(helper).size(), model.linksForSymbol(helper).size());
        QCOMPARE(reopened.appearances(helper).size(), 2);
        const QString group = reopened.symbol("demo.cpp::calculate_beta(int)", "head")
                                  .value("candidateGroup").toString();
        QVERIFY(!group.isEmpty());
        QCOMPARE(group, reopened.symbol("demo.cpp::calculate_with_state(int)", "head")
                          .value("candidateGroup").toString());
        QVERIFY(reopened.evidenceErrors().isEmpty());

        QJsonObject tampered = model.document();
        QJsonArray builds = tampered.value("builds").toArray();
        for (int i = 0; i < builds.size(); ++i) {
            QJsonObject build = builds.at(i).toObject();
            if (build.value("id").toString() != o0) continue;
            build["binarySha256"] = "wrong hash";
            builds[i] = build;
        }
        tampered["builds"] = builds;
        tampered["selectedBuildId"] = o0;
        tampered["evidenceRoot"] = model.root();
        QFile bad(dir.filePath("tampered.json"));
        QVERIFY(bad.open(QIODevice::WriteOnly));
        bad.write(QJsonDocument(tampered).toJson());
        bad.close();
        TraceModel invalid;
        QVERIFY(invalid.load(bad.fileName()));
        QVERIFY(!invalid.evidenceErrors().isEmpty());
        QCOMPARE(invalid.range(helper).value("status").toString(), QString("unverified-evidence"));
        QVERIFY(invalid.sourceBinary().findAllBySourceLine("head/demo.cpp", 6, o0).isEmpty());
    }

    void interfaceTransitionsAndScreenshot()
    {
        TraceExplorer explorer;
        QString error;
        QVERIFY2(explorer.openManifest(manifest(), &error), qPrintable(error));
        explorer.show();
        (void)QTest::qWaitForWindowExposed(&explorer);
        QVERIFY(explorer.selectNode("requirement", "REQ-ONE"));
        QVERIFY(explorer.detailsText().contains("scenario_one"));
        QVERIFY(explorer.detailsText().contains("shared_helper"));
        QVERIFY(explorer.graphNodeCount() >= 3);
        auto* graph = explorer.findChild<QGraphicsView*>("traceGraph");
        QVERIFY(graph);
        bool clickedHelper = false;
        for (QGraphicsItem* item : graph->scene()->items()) {
            if (item->data(0).toString() != "symbol" ||
                item->data(1).toString() != "demo.cpp::shared_helper(int)") continue;
            const QPoint location = graph->mapFromScene(item->mapToScene(item->boundingRect().center()));
            QTest::mouseClick(graph->viewport(), Qt::LeftButton, Qt::NoModifier, location);
            clickedHelper = true;
            break;
        }
        QVERIFY(clickedHelper);
        QCOMPARE(explorer.selectedNodeId(), QString("demo.cpp::shared_helper(int)"));
        QVERIFY(explorer.detailsText().contains("REQ-ONE"));
        QVERIFY(explorer.detailsText().contains("REQ-TWO"));
        QVERIFY(explorer.detailsText().contains("one-helper"));
        QVERIFY(explorer.detailsText().contains("two-helper"));
        QVERIFY(explorer.detailsText().contains("0 manifest addresses missing"));
        auto* card = explorer.findChild<QTextBrowser*>("traceCard");
        QVERIFY(card);
        QVERIFY(QMetaObject::invokeMethod(card, "anchorClicked", Qt::DirectConnection,
                                          Q_ARG(QUrl, QUrl("trace:diff"))));
        bool diffOpened = false;
        for (QDialog* dialog : explorer.findChildren<QDialog*>()) {
            if (!dialog->windowTitle().contains("Git diff")) continue;
            diffOpened = dialog->findChild<QTextBrowser*>()->toPlainText()
                             .contains("+    return calculate_beta(input) + shared_helper(input);");
            dialog->close();
        }
        QVERIFY(diffOpened);
        TraceModel evidence;
        QVERIFY(evidence.load(manifest()));
        const auto helperRange = evidence.range("demo.cpp::shared_helper(int)");
        const QString instruction = helperRange.value("section").toString() + ':' +
                                    helperRange.value("instructions").toArray().first().toString();
        QVERIFY(explorer.selectNode("instruction", instruction));
        QVERIFY(explorer.detailsText().contains("head/demo.cpp"));
        QVERIFY(explorer.selectNode("source-line", "head/demo.cpp:6"));
        QVERIFY(explorer.detailsText().contains("5 instruction interval(s)"));

        auto* tree = explorer.findChild<QTreeWidget*>("traceTree");
        QVERIFY(tree);
        auto matches = tree->findItems("REQ-CALC*", Qt::MatchWildcard | Qt::MatchRecursive);
        QVERIFY(!matches.isEmpty());
        tree->scrollToItem(matches.first());
        const QRect rect = tree->visualItemRect(matches.first());
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());
        QCOMPARE(explorer.selectedNodeId(), QString("REQ-CALC"));
        QVERIFY(explorer.detailsText().contains("calculate_alpha"));
        QVERIFY(explorer.detailsText().contains("calculate_beta"));
        const int fullGraph = explorer.graphNodeCount();
        auto* groupFilter = explorer.findChild<QComboBox*>("traceGroupFilter");
        auto* changedFilter = explorer.findChild<QCheckBox*>("traceChangedFilter");
        QVERIFY(groupFilter && changedFilter);
        const QString exactGroup = evidence.symbol("demo.cpp::calculate_alpha(int)", "head")
                                       .value("exactGroup").toString();
        groupFilter->setCurrentIndex(groupFilter->findData(exactGroup));
        QVERIFY(explorer.graphNodeCount() < fullGraph);
        changedFilter->setChecked(true);
        QCOMPARE(explorer.graphNodeCount(), 1); // Only the requirement remains.
        changedFilter->setChecked(false);
        groupFilter->setCurrentIndex(0);

        auto* direction = explorer.findChild<QComboBox*>("traceDirectionFilter");
        auto* depth = explorer.findChild<QSpinBox*>("traceDepthFilter");
        auto* reqFilter = explorer.findChild<QComboBox*>("traceRequirementFilter");
        QVERIFY(direction && depth && reqFilter);
        QVERIFY(explorer.selectNode("requirement", "REQ-ONE"));
        direction->setCurrentIndex(1); // Incoming: no outgoing requirement links.
        QCOMPARE(explorer.graphNodeCount(), 1);
        direction->setCurrentIndex(2);
        QVERIFY(explorer.graphNodeCount() > 1);
        direction->setCurrentIndex(0);
        QVERIFY(explorer.selectNode("symbol", "demo.cpp::shared_helper(int)"));
        depth->setValue(1);
        const int shallow = explorer.graphNodeCount();
        depth->setValue(3);
        QVERIFY(explorer.graphNodeCount() > shallow);
        const int expanded = explorer.graphNodeCount();
        reqFilter->setCurrentIndex(reqFilter->findData("REQ-TWO"));
        QVERIFY(explorer.graphNodeCount() < expanded);
        reqFilter->setCurrentIndex(0);
        depth->setValue(2);

        QVERIFY(explorer.selectNode("symbol", "demo.cpp::calculate_with_state(int)"));
        QVERIFY(explorer.detailsText().contains("extra write to volatile state"));
        QVERIFY(explorer.selectNode("symbol", "demo.cpp::removed_probe(int)"));
        QVERIFY(explorer.detailsText().contains("Deleted in head"));
        QVERIFY(explorer.selectNode("symbol", "demo.cpp::shared_helper(int)"));
        const QString screenshot = QStringLiteral(TRACE_FIXTURE_ROOT) + "/artifacts/trace-explorer.png";
        QVERIFY(explorer.grab().save(screenshot));

        auto* combo = explorer.findChild<QComboBox*>("traceBuildSelector");
        QVERIFY(combo);
        int o2Index = -1;
        for (int i = 0; i < combo->count(); ++i)
            if (combo->itemText(i).startsWith("head O2")) o2Index = i;
        QVERIFY(o2Index >= 0);
        combo->setCurrentIndex(o2Index);
        QVERIFY(explorer.selectNode("symbol", "demo.cpp::debug_only(int)"));
        QVERIFY(explorer.detailsText().contains("Optimized away or unavailable"));
        int noDebugIndex = -1;
        for (int i = 0; i < combo->count(); ++i)
            if (combo->itemText(i).contains("O2-nog")) noDebugIndex = i;
        QVERIFY(noDebugIndex >= 0);
        combo->setCurrentIndex(noDebugIndex);
        QVERIFY(explorer.selectNode("symbol", "demo.cpp::shared_helper(int)"));
        QVERIFY(explorer.detailsText().contains("No debug information"));
        QVERIFY(explorer.selectNode("source-line", "head/demo.cpp:6"));
        QVERIFY(explorer.detailsText().contains("0 instruction interval(s)"));
        int baseIndex = -1;
        for (int i = 0; i < combo->count(); ++i)
            if (combo->itemText(i).startsWith("base O0")) baseIndex = i;
        QVERIFY(baseIndex >= 0);
        combo->setCurrentIndex(baseIndex);
        QVERIFY(explorer.selectNode("symbol", "demo.cpp::shared_helper(int)"));
        QVERIFY(!explorer.detailsText().contains("one-helper"));
        auto traceItems = tree->findItems("*one-helper*", Qt::MatchWildcard | Qt::MatchRecursive);
        QVERIFY(!traceItems.isEmpty());
        tree->expandItem(traceItems.first()->parent());
        tree->scrollToItem(traceItems.first());
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier,
                          tree->visualItemRect(traceItems.first()).center());
        QVERIFY(combo->currentText().startsWith("head O0"));
        QVERIFY(explorer.detailsText().contains("one-helper"));
    }
};

QTEST_MAIN(TestTraceExplorer)
#include "test_trace_explorer.moc"
