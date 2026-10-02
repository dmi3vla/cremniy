#include "trace_explorer.h"
#include "ToolTabs/Disassembler/disassemblerworker.h"
#include <QCheckBox>
#include <QCryptographicHash>
#include <QComboBox>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QDir>
#include <QSet>

namespace {
QString esc(const QString& text) { return text.toHtmlEscaped(); }
QString key(const QString& kind, const QString& id) { return kind + '|' + id; }
QString hex(quint64 value) { return QStringLiteral("0x") + QString::number(value, 16); }
QColor groupColor(const QString& id)
{
    const QByteArray digest = QCryptographicHash::hash(id.toUtf8(), QCryptographicHash::Sha256);
    static const QColor palette[] = {QColor("#62d6ff"), QColor("#ffc46b"), QColor("#eaa8ff"),
                                     QColor("#9de590"), QColor("#ff8fb2")};
    return palette[static_cast<unsigned char>(digest.at(0)) % 5];
}
QString badgeText(const QString& state)
{
    if (state == "added") return QStringLiteral("+ добавлено");
    if (state == "modified") return QStringLiteral("M изменено");
    if (state == "deleted") return QStringLiteral("− удалено");
    return {};
}
}

TraceExplorer::TraceExplorer(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Visual trace prototype"));
    resize(1350, 850);
    auto* outer = new QVBoxLayout(this);
    auto* controls = new QHBoxLayout;
    auto* open = new QPushButton(tr("Open manifest"), this);
    auto* save = new QPushButton(tr("Save session"), this);
    m_build = new QComboBox(this);
    m_build->setObjectName("traceBuildSelector");
    m_group = new QComboBox(this);
    m_group->setObjectName("traceGroupFilter");
    m_requirement = new QComboBox(this);
    m_requirement->setObjectName("traceRequirementFilter");
    m_direction = new QComboBox(this);
    m_direction->setObjectName("traceDirectionFilter");
    m_direction->addItems({tr("Both"), tr("Incoming"), tr("Outgoing")});
    m_changed = new QCheckBox(tr("Changed only"), this);
    m_changed->setObjectName("traceChangedFilter");
    m_depth = new QSpinBox(this);
    m_depth->setObjectName("traceDepthFilter");
    m_depth->setRange(1, 4);
    m_depth->setValue(2);
    controls->addWidget(open);
    controls->addWidget(save);
    controls->addWidget(new QLabel(tr("Build:"), this));
    controls->addWidget(m_build, 1);
    controls->addWidget(new QLabel(tr("Group:"), this));
    controls->addWidget(m_group);
    controls->addWidget(new QLabel(tr("Requirement:"), this));
    controls->addWidget(m_requirement);
    controls->addWidget(m_changed);
    controls->addWidget(m_direction);
    controls->addWidget(new QLabel(tr("Depth:"), this));
    controls->addWidget(m_depth);
    outer->addLayout(controls);

    m_legend = new QLabel(tr("Solid contour + group ID: same symbol in multiple traces. "
                           "Dashed contour + group ID: comparison candidates. "
                           "Blue fill: selected. Separate badge: Git status."), this);
    m_legend->setWordWrap(true);
    outer->addWidget(m_legend);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    m_tree = new QTreeWidget(splitter);
    m_tree->setObjectName("traceTree");
    m_tree->setHeaderLabel(tr("Project / requirements / traces"));
    m_scene = new QGraphicsScene(this);
    m_graph = new QGraphicsView(m_scene, splitter);
    m_graph->setObjectName("traceGraph");
    m_graph->setRenderHint(QPainter::Antialiasing);
    m_graph->setDragMode(QGraphicsView::ScrollHandDrag);
    m_details = new QTextBrowser(splitter);
    m_details->setObjectName("traceCard");
    m_details->setOpenLinks(false);
    splitter->addWidget(m_tree);
    splitter->addWidget(m_graph);
    splitter->addWidget(m_details);
    splitter->setSizes({250, 640, 440});
    outer->addWidget(splitter, 1);

    connect(open, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, tr("Trace manifest"), m_model.root(),
                                                           tr("JSON (*.json)"));
        if (path.isEmpty()) return;
        QString error;
        if (!openManifest(path, &error)) QMessageBox::warning(this, tr("Trace"), error);
    });
    connect(save, &QPushButton::clicked, this, [this] {
        if (m_model.root().isEmpty()) return;
        const QString path = QFileDialog::getSaveFileName(
            this, tr("Save trace session"), QDir(m_model.root()).filePath("trace-session.json"),
            tr("JSON (*.json)"));
        if (path.isEmpty()) return;
        QString error;
        if (!m_model.save(path, &error)) QMessageBox::warning(this, tr("Trace"), error);
    });
    connect(m_build, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index < 0 || !m_model.selectBuild(m_build->itemData(index).toString())) return;
        verifyBackend();
        rebuildTree();
        rebuildGraph();
        showDetails();
    });
    connect(m_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int) {
        const QString kind = item->data(0, Qt::UserRole).toString();
        const QString id = item->data(0, Qt::UserRole + 1).toString();
        const QString revision = item->data(0, Qt::UserRole + 2).toString();
        if (!revision.isEmpty() && revision != m_model.selectedBuild().value("revision").toString()) {
            const QString optimization = m_model.selectedBuild().value("optimization").toString();
            int fallback = -1;
            bool exactFound = false;
            for (int i = 0; i < m_build->count(); ++i) {
                for (const auto& value : m_model.document().value("builds").toArray()) {
                    const auto build = value.toObject();
                    if (build.value("id").toString() != m_build->itemData(i).toString() ||
                        build.value("revision").toString() != revision) continue;
                    if (fallback < 0) fallback = i;
                    if (!exactFound && build.value("optimization").toString() == optimization &&
                        build.value("hasDebugInfo").toBool()) {
                        fallback = i;
                        exactFound = true;
                    }
                }
            }
            if (fallback >= 0) m_build->setCurrentIndex(fallback);
        }
        if (!kind.isEmpty()) choose(kind, id);
    });
    connect(m_scene, &QGraphicsScene::selectionChanged, this, [this] {
        if (m_rendering || m_scene->selectedItems().isEmpty()) return;
        const auto* item = m_scene->selectedItems().first();
        choose(item->data(0).toString(), item->data(1).toString());
    });
    connect(m_details, &QTextBrowser::anchorClicked, this, [this](const QUrl& url) {
        const QString value = url.toString();
        if (value == "trace:diff") {
            auto* dialog = new QDialog(this);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->setWindowTitle(tr("Real Git diff: base → head"));
            dialog->resize(900, 650);
            auto* layout = new QVBoxLayout(dialog);
            auto* view = new QTextBrowser(dialog);
            view->setPlainText(m_model.diffText());
            layout->addWidget(view);
            dialog->show();
        } else if (value == "trace:disassembly") {
            auto* dialog = new QDialog(this);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->setWindowTitle(tr("Saved objdump output"));
            dialog->resize(900, 650);
            auto* layout = new QVBoxLayout(dialog);
            auto* view = new QTextBrowser(dialog);
            const QString filePath = QDir(m_model.root()).filePath(
                m_model.selectedBuild().value("disassembly").toString());
            QFile file(filePath);
            if (file.open(QIODevice::ReadOnly)) view->setPlainText(QString::fromUtf8(file.readAll()));
            layout->addWidget(view);
            dialog->show();
        } else if (value.startsWith("trace:symbol:")) {
            choose("symbol", value.mid(QString("trace:symbol:").size()));
        } else if (value.startsWith("trace:requirement:")) {
            choose("requirement", value.mid(QString("trace:requirement:").size()));
        } else if (value.startsWith("trace:instruction:")) {
            choose("instruction", value.mid(QString("trace:instruction:").size()));
        } else if (value.startsWith("trace:source-line:")) {
            choose("source-line", value.mid(QString("trace:source-line:").size()));
        }
    });
    for (QComboBox* combo : {m_group, m_requirement, m_direction})
        connect(combo, &QComboBox::currentIndexChanged, this, [this] { rebuildGraph(); });
    connect(m_changed, &QCheckBox::toggled, this, [this] { rebuildGraph(); });
    connect(m_depth, &QSpinBox::valueChanged, this, [this] { rebuildGraph(); });
}

bool TraceExplorer::openManifest(const QString& path, QString* error)
{
    if (!m_model.load(path, error)) return false;
    m_build->blockSignals(true);
    m_build->clear();
    for (const auto& value : m_model.document().value("builds").toArray()) {
        const auto build = value.toObject();
        m_build->addItem(build.value("revision").toString() + ' ' +
                         build.value("optimization").toString() + " · " + build.value("id").toString(),
                         build.value("id").toString());
    }
    m_build->setCurrentIndex(m_build->findData(m_model.selectedBuildId()));
    m_build->blockSignals(false);

    m_group->blockSignals(true);
    m_group->clear();
    m_group->addItem(tr("All groups"), QString());
    QSet<QString> groups;
    for (const auto& value : m_model.document().value("symbols").toArray()) {
        const auto item = value.toObject();
        if (!item.value("exactGroup").toString().isEmpty()) groups.insert(item.value("exactGroup").toString());
        if (!item.value("candidateGroup").toString().isEmpty()) groups.insert(item.value("candidateGroup").toString());
    }
    for (const auto& group : groups) m_group->addItem(group, group);
    m_group->blockSignals(false);

    m_requirement->blockSignals(true);
    m_requirement->clear();
    m_requirement->addItem(tr("All requirements"), QString());
    for (const auto& value : m_model.document().value("requirements").toArray()) {
        const auto item = value.toObject();
        m_requirement->addItem(item.value("id").toString(), item.value("id").toString());
    }
    m_requirement->blockSignals(false);
    m_selectedKind = "requirement";
    const auto requirements = m_model.document().value("requirements").toArray();
    m_selectedId = requirements.isEmpty() ? QString() : requirements.first().toObject().value("id").toString();
    verifyBackend();
    rebuildTree();
    rebuildGraph();
    showDetails();
    return true;
}

bool TraceExplorer::selectNode(const QString& kind, const QString& id)
{
    bool exists = kind == "requirement" ? !m_model.linksForRequirement(id).isEmpty() :
                  kind == "symbol" ? !m_model.symbol(id, m_model.selectedBuild().value("revision").toString()).isEmpty() ||
                                     !m_model.symbol(id, "base").isEmpty() : false;
    if (kind == "source-line") exists = id.lastIndexOf(':') > 0;
    if (kind == "instruction" && m_model.isSelectedBuildVerified()) {
        for (const auto& value : m_model.selectedBuild().value("ranges").toArray()) {
            const auto range = value.toObject();
            for (const auto& address : range.value("instructions").toArray())
                if (range.value("section").toString() + ':' + address.toString() == id) exists = true;
        }
    }
    if (!exists) return false;
    choose(kind, id);
    return true;
}

QString TraceExplorer::detailsText() const { return m_details->toPlainText(); }
int TraceExplorer::graphNodeCount() const
{
    int count = 0;
    for (const auto* item : m_scene->items())
        if (item->data(0).isValid()) ++count;
    return count;
}

void TraceExplorer::choose(const QString& kind, const QString& id)
{
    if (kind.isEmpty() || id.isEmpty()) return;
    m_selectedKind = kind;
    m_selectedId = id;
    const auto item = m_treeItems.value(key(kind, id), nullptr);
    if (item) m_tree->setCurrentItem(item);
    rebuildGraph();
    showDetails();
}

void TraceExplorer::rebuildTree()
{
    m_tree->clear();
    m_treeItems.clear();
    const auto doc = m_model.document();
    auto* requirements = new QTreeWidgetItem(m_tree, {tr("Requirements")});
    for (const auto& value : doc.value("requirements").toArray()) {
        const auto req = value.toObject();
        const QString id = req.value("id").toString();
        auto* item = new QTreeWidgetItem(requirements, {id + " · " + req.value("title").toString()});
        item->setData(0, Qt::UserRole, "requirement");
        item->setData(0, Qt::UserRole + 1, id);
        m_treeItems.insert(key("requirement", id), item);
    }
    auto* traces = new QTreeWidgetItem(m_tree, {tr("Codemap traces")});
    for (const auto& trace : m_model.codemap().traces) {
        auto* branch = new QTreeWidgetItem(traces, {trace.id + " · " + trace.title});
        for (const auto& location : trace.locations) {
            const QString id = "demo.cpp::" + location.title + "(int)";
            auto* item = new QTreeWidgetItem(branch, {location.title + " · " + location.id +
                                                     " @ " + location.path});
            item->setData(0, Qt::UserRole, "symbol");
            item->setData(0, Qt::UserRole + 1, id);
            item->setData(0, Qt::UserRole + 2, location.path.section('/', 0, 0));
        }
    }
    auto* sources = new QTreeWidgetItem(m_tree, {tr("Source / symbols")});
    const QString revision = m_model.selectedBuild().value("revision").toString();
    auto* source = new QTreeWidgetItem(sources, {revision + "/demo.cpp"});
    for (const auto& value : m_model.symbolsForRevision(revision)) {
        const auto symbol = value.toObject();
        const QString id = symbol.value("id").toString();
        const QString state = badgeText(symbol.value("gitState").toString());
        auto* item = new QTreeWidgetItem(source, {symbol.value("name").toString() +
                                                   (state.isEmpty() ? "" : " [" + state + ']')});
        item->setData(0, Qt::UserRole, "symbol");
        item->setData(0, Qt::UserRole + 1, id);
        m_treeItems.insert(key("symbol", id), item);
    }
    if (revision == "head") {
        auto* removed = new QTreeWidgetItem(sources, {tr("Deleted since base")});
        for (const auto& value : m_model.symbolsForRevision("base")) {
            const auto symbol = value.toObject();
            if (symbol.value("gitState").toString() != "deleted") continue;
            const QString id = symbol.value("id").toString();
            auto* item = new QTreeWidgetItem(removed, {symbol.value("name").toString() +
                                                       " [" + badgeText("deleted") + ']'});
            item->setData(0, Qt::UserRole, "symbol");
            item->setData(0, Qt::UserRole + 1, id);
            m_treeItems.insert(key("symbol", id), item);
        }
    }
    auto* binary = new QTreeWidgetItem(m_tree, {tr("Binary / instructions")});
    auto* build = new QTreeWidgetItem(binary, {m_model.selectedBuildId()});
    const auto displayedRanges = m_model.isSelectedBuildVerified()
                                     ? m_model.selectedBuild().value("ranges").toArray() : QJsonArray{};
    for (const auto& rangeValue : displayedRanges) {
        const auto range = rangeValue.toObject();
        const QString id = range.value("symbolId").toString();
        auto* symbol = new QTreeWidgetItem(build, {id.section("::", 1, 1) + " · " +
                                                     range.value("status").toString()});
        symbol->setData(0, Qt::UserRole, "symbol");
        symbol->setData(0, Qt::UserRole + 1, id);
        for (const auto& addressValue : range.value("instructions").toArray()) {
            const QString instructionId = range.value("section").toString() + ':' + addressValue.toString();
            auto* instruction = new QTreeWidgetItem(symbol, {instructionId});
            instruction->setData(0, Qt::UserRole, "instruction");
            instruction->setData(0, Qt::UserRole + 1, instructionId);
            m_treeItems.insert(key("instruction", instructionId), instruction);
        }
    }
    m_tree->expandItem(requirements);
    m_tree->expandItem(traces);
    m_tree->expandItem(sources);
}

QGraphicsRectItem* TraceExplorer::addGraphNode(const QString& kind, const QString& id,
                                                const QString& label, const QPointF& position,
                                                const QString& groupId, bool repeated,
                                                const QString& gitState)
{
    const QRectF box(position, QSizeF(235, 64));
    auto* item = m_scene->addRect(box);
    item->setFlag(QGraphicsItem::ItemIsSelectable);
    item->setData(0, kind);
    item->setData(1, id);
    const bool selected = kind == m_selectedKind && id == m_selectedId;
    item->setBrush(selected ? QColor(35, 72, 112) : QColor(38, 42, 54));
    QPen pen(groupId.isEmpty() ? QColor(105, 112, 126) : groupColor(groupId), repeated ? 3 : 2);
    if (!groupId.isEmpty() && !repeated) pen.setStyle(Qt::DashLine);
    item->setPen(pen);
    auto* text = m_scene->addSimpleText(label);
    text->setBrush(Qt::white);
    text->setPos(position + QPointF(7, 4));
    text->setAcceptedMouseButtons(Qt::NoButton);
    if (!groupId.isEmpty()) {
        auto* group = m_scene->addSimpleText(groupId);
        group->setBrush(groupColor(groupId));
        group->setPos(position + QPointF(7, 26));
        group->setAcceptedMouseButtons(Qt::NoButton);
    }
    if (!gitState.isEmpty() && gitState != "unchanged") {
        const QColor badgeColor = gitState == "added" ? QColor("#82d68a") :
                                  gitState == "deleted" ? QColor("#ee8888") : QColor("#edc675");
        auto* background = m_scene->addRect(
            QRectF(position.x() + 7, position.y() + 43, 115, 17), Qt::NoPen, badgeColor);
        background->setAcceptedMouseButtons(Qt::NoButton);
        auto* badge = m_scene->addSimpleText(badgeText(gitState));
        badge->setBrush(QColor("#20232d"));
        badge->setPos(position + QPointF(9, 42));
        badge->setAcceptedMouseButtons(Qt::NoButton);
    }
    return item;
}

void TraceExplorer::rebuildGraph()
{
    m_rendering = true;
    m_scene->clear();
    const auto build = m_model.selectedBuild();
    const QString revision = build.value("revision").toString();
    QJsonArray allSymbols = m_model.symbolsForRevision(revision);
    if (revision == "head") {
        for (const auto& value : m_model.symbolsForRevision("base"))
            if (value.toObject().value("gitState").toString() == "deleted") allSymbols.append(value);
    }
    QSet<QString> visible;
    QSet<QString> requirements;
    const QString reqFilter = m_requirement->currentData().toString();
    const QString groupFilter = m_group->currentData().toString();
    const auto links = m_model.document().value("links").toArray();

    if (m_selectedKind == "requirement") requirements.insert(m_selectedId);
    else if (m_selectedKind == "symbol") visible.insert(m_selectedId);
    else if (m_selectedKind == "source-line") {
        const int colon = m_selectedId.lastIndexOf(':');
        if (colon > 0) {
            for (const auto& mapping : m_model.sourceBinary().findAllBySourceLine(
                     m_selectedId.left(colon), m_selectedId.mid(colon + 1).toInt(), m_model.selectedBuildId()))
                visible.insert(mapping.functionName);
        }
    }
    else if (m_selectedKind == "instruction" && m_model.isSelectedBuildVerified()) {
        for (const auto& value : build.value("ranges").toArray()) {
            const auto range = value.toObject();
            if (range.value("section").toString() + ':' + range.value("start").toString() == m_selectedId)
                visible.insert(range.value("symbolId").toString());
            for (const auto& address : range.value("instructions").toArray())
                if (range.value("section").toString() + ':' + address.toString() == m_selectedId)
                    visible.insert(range.value("symbolId").toString());
        }
    }
    const int direction = m_direction->currentIndex();
    for (int depth = 0; depth < m_depth->value(); ++depth) {
        QSet<QString> nextSymbols = visible;
        QSet<QString> nextReqs = requirements;
        for (const auto& value : links) {
            const auto link = value.toObject();
            const QString kind = link.value("kind").toString();
            if (kind == "requirement-symbol") {
                const QString req = link.value("requirementId").toString();
                const QString sym = link.value("symbolId").toString();
                if (requirements.contains(req) && direction != 1) nextSymbols.insert(sym);
                if (visible.contains(sym) && direction != 2) nextReqs.insert(req);
            } else if (kind == "calls" && link.value("revision").toString() == revision) {
                const QString from = link.value("from").toString();
                const QString to = link.value("to").toString();
                if (visible.contains(from) && direction != 1) nextSymbols.insert(to);
                if (visible.contains(to) && direction != 2) nextSymbols.insert(from);
            }
        }
        visible = nextSymbols;
        requirements = nextReqs;
    }
    if (!reqFilter.isEmpty()) {
        QSet<QString> allowed;
        for (const auto& value : m_model.linksForRequirement(reqFilter))
            allowed.insert(value.toObject().value("symbolId").toString());
        visible.intersect(allowed);
        requirements = {reqFilter};
    }
    QMap<QString, QJsonObject> symbols;
    for (const auto& value : allSymbols) {
        const auto symbol = value.toObject();
        const QString id = symbol.value("id").toString();
        if (!visible.contains(id)) continue;
        if (!groupFilter.isEmpty() && symbol.value("exactGroup").toString() != groupFilter &&
            symbol.value("candidateGroup").toString() != groupFilter) continue;
        if (m_changed->isChecked() && symbol.value("gitState").toString() == "unchanged") continue;
        symbols.insert(id, symbol);
    }

    QMap<QString, QPointF> positions;
    int y = 20;
    for (const auto& req : requirements) {
        const QString keyName = "requirement|" + req;
        positions.insert(keyName, QPointF(20, y));
        addGraphNode("requirement", req, req, positions.value(keyName));
        y += 95;
    }
    y = 20;
    for (auto it = symbols.begin(); it != symbols.end(); ++it) {
        const auto symbol = it.value();
        const QString group = symbol.value("candidateGroup").toString();
        const QString repeatedGroup = m_model.appearances(it.key(), revision).size() > 1 ? it.key() : QString();
        const QString borderGroup = repeatedGroup.isEmpty() ? group : repeatedGroup;
        const QString keyName = "symbol|" + it.key();
        positions.insert(keyName, QPointF(310, y));
        addGraphNode("symbol", it.key(), symbol.value("name").toString(), positions.value(keyName),
                     borderGroup, !repeatedGroup.isEmpty(), symbol.value("gitState").toString());
        y += 100;
    }
    y = 20;
    for (auto it = symbols.begin(); it != symbols.end(); ++it) {
        const QStringList appearances = m_model.appearances(it.key(), revision);
        if (appearances.size() < 2) continue;
        for (const auto& appearance : appearances) {
            const QString id = it.key();
            const QString graphKey = "appearance|" + appearance;
            positions.insert(graphKey, QPointF(610, y));
            addGraphNode("symbol", id, appearance.section(" @", 0, 0), positions.value(graphKey), id, true);
            y += 88;
        }
    }
    for (const auto& value : links) {
        const auto link = value.toObject();
        QString from, to;
        if (link.value("kind").toString() == "requirement-symbol") {
            from = "requirement|" + link.value("requirementId").toString();
            to = "symbol|" + link.value("symbolId").toString();
        } else if (link.value("kind").toString() == "calls" && link.value("revision").toString() == revision) {
            from = "symbol|" + link.value("from").toString();
            to = "symbol|" + link.value("to").toString();
        }
        if (positions.contains(from) && positions.contains(to)) {
            const auto first = positions.value(from) + QPointF(235, 32);
            const auto second = positions.value(to) + QPointF(0, 32);
            auto* edge = m_scene->addLine(QLineF(first, second), QPen(QColor("#8295a8"), 1));
            edge->setZValue(-1);
        }
    }
    for (auto it = positions.begin(); it != positions.end(); ++it) {
        if (!it.key().startsWith("appearance|")) continue;
        const auto appearance = it.key().mid(QString("appearance|").size());
        for (auto sym = symbols.begin(); sym != symbols.end(); ++sym) {
            if (!m_model.appearances(sym.key(), revision).contains(appearance)) continue;
            auto* edge = m_scene->addLine(QLineF(positions.value("symbol|" + sym.key()) + QPointF(235, 32),
                                                 it.value() + QPointF(0, 32)), QPen(groupColor(sym.key()), 1));
            edge->setZValue(-1);
        }
    }
    m_scene->setSceneRect(m_scene->itemsBoundingRect().adjusted(-20, -20, 40, 40));
    if (!m_scene->sceneRect().isEmpty())
        m_graph->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
    m_rendering = false;
}

QString TraceExplorer::requirementCard(const QString& id) const
{
    QString html = "<h2>" + esc(id) + "</h2>";
    for (const auto& value : m_model.document().value("requirements").toArray()) {
        const auto req = value.toObject();
        if (req.value("id").toString() == id) html += "<p>" + esc(req.value("title").toString()) + "</p>";
    }
    html += "<h3>Связанные функции</h3><ul>";
    for (const auto& value : m_model.linksForRequirement(id)) {
        const auto link = value.toObject();
        if (link.value("kind").toString() != "requirement-symbol") continue;
        const QString symbol = link.value("symbolId").toString();
        html += "<li><a href=\"trace:symbol:" + esc(symbol) + "\">" + esc(symbol) + "</a> " +
                esc(link.value("kind").toString()) + " · " + esc(link.value("originKind").toString()) +
                ':' + esc(link.value("origin").toString()) + "</li>";
    }
    html += "</ul><h3>Source spans</h3><ul>";
    for (const auto& value : m_model.linksForRequirement(id)) {
        const auto link = value.toObject();
        if (link.value("kind").toString() != "requirement-source") continue;
        const auto source = link.value("source").toObject();
        html += "<li>" + esc(source.value("file").toString()) + ':' +
                QString::number(source.value("startLine").toInt()) + '-' +
                QString::number(source.value("endLine").toInt()) + " · " +
                esc(source.value("revision").toString()) + " · " +
                esc(link.value("originKind").toString()) + ':' +
                esc(link.value("origin").toString()) + "</li>";
    }
    return html + "</ul>";
}

QString TraceExplorer::symbolCard(const QString& id) const
{
    const auto build = m_model.selectedBuild();
    const QString revision = build.value("revision").toString();
    QJsonObject symbol = m_model.symbol(id, revision);
    const bool deleted = symbol.isEmpty() && revision == "head" &&
                         m_model.symbol(id, "base").value("gitState").toString() == "deleted";
    if (deleted) symbol = m_model.symbol(id, "base");
    if (symbol.isEmpty()) return tr("Symbol absent in selected revision");
    QString html = "<h2>" + esc(symbol.value("name").toString()) + "</h2>";
    const QString symbolRevision = symbol.value("revision").toString();
    const auto sourceHash = m_model.document().value("revisions").toObject()
                                .value(symbolRevision).toObject().value("sha256").toString();
    html += "<p><b>ID:</b> " + esc(id) + "<br><b>Revision:</b> " + esc(symbolRevision) +
            "<br><b>Source SHA256:</b> " + esc(sourceHash) +
            "<br><b>Location:</b> " + esc(symbol.value("path").toString()) + ':' +
            QString::number(symbol.value("startLine").toInt()) + '-' +
            QString::number(symbol.value("endLine").toInt()) + "<br><b>Git:</b> " +
            esc(symbol.value("gitState").toString()) + "</p>";
    html += "<p><a href=\"trace:diff\">Open real base/head Git diff</a></p>";
    html += "<h3>Requirements and provenance</h3><ul>";
    for (const auto& value : m_model.linksForSymbol(id)) {
        const auto link = value.toObject();
        if (link.value("kind").toString() != "requirement-symbol") continue;
        const QString req = link.value("requirementId").toString();
        html += "<li><a href=\"trace:requirement:" + esc(req) + "\">" + esc(req) +
                "</a> · " + esc(link.value("originKind").toString()) + ':' +
                esc(link.value("origin").toString()) + "</li>";
    }
    html += "</ul><h3>Codemap appearances</h3><p>" +
            esc(m_model.appearances(id, symbolRevision).join(", ")) + "</p>";
    const QString exact = symbol.value("exactGroup").toString();
    const QString candidate = symbol.value("candidateGroup").toString();
    html += "<h3>Comparison</h3><p>Exact group: " + esc(exact.isEmpty() ? "—" : exact) +
            "<br>Candidate group: " + esc(candidate.isEmpty() ? "—" : candidate) +
            "<br>Basis: " + esc(symbol.value("comparison").toString()) +
            "<br>Code similarity is not proof of equal behaviour.</p>";
    const auto lines = m_model.sourceText(symbolRevision).split('\n');
    html += "<h3>Source</h3><pre>";
    for (int line = symbol.value("startLine").toInt(); line <= symbol.value("endLine").toInt() && line <= lines.size(); ++line) {
        if (line <= 0) continue;
        const QString lineId = symbol.value("path").toString() + ':' + QString::number(line);
        html += "<a href=\"trace:source-line:" + esc(lineId) + "\">" + QString::number(line) +
                "</a>  " + esc(lines.at(line - 1)) + '\n';
    }
    html += "</pre>";
    html += "<h3>Binary: " + esc(m_model.selectedBuildId()) + "</h3>";
    html += "<p><a href=\"trace:disassembly\">Open saved objdump output</a></p>";
    QStringList flags;
    for (const auto& value : build.value("flags").toArray()) flags << value.toString();
    html += "<p>Compiler: " + esc(build.value("compiler").toString()) +
            "<br>Flags: " + esc(flags.join(' ')) +
            "<br>Architecture: " + esc(build.value("architecture").toString()) +
            "<br>Binary SHA256: " + esc(build.value("binarySha256").toString()) + "</p>";
    const auto range = m_model.range(id);
    if (deleted) return html + "<p>Deleted in head: no head binary address.</p>";
    if (!build.value("hasDebugInfo").toBool()) html += "<p>No debug information.</p>";
    if (range.value("status").toString() == "unverified-evidence")
        html += "<p>Evidence mismatch: instruction addresses suppressed.</p>";
    else if (range.value("status").toString() != "mapped")
        html += "<p>Optimized away or unavailable: no verified instruction range.</p>";
    else {
        html += "<p>" + esc(range.value("section").toString()) + " · [" +
                esc(range.value("start").toString()) + ", " + esc(range.value("end").toString()) +
                ") · source: nm/readelf + DWARF/addr2line</p><ul>";
        int shown = 0;
        for (const auto& value : range.value("instructions").toArray()) {
            if (shown++ >= 80) { html += "<li>…</li>"; break; }
            const QString instruction = range.value("section").toString() + ':' + value.toString();
            html += "<li><a href=\"trace:instruction:" + esc(instruction) + "\">" +
                    esc(instruction) + "</a></li>";
        }
        html += "</ul>";
    }
    return html;
}

QString TraceExplorer::instructionCard(const QString& id) const
{
    const QString section = id.section(':', 0, 0);
    bool ok = false;
    const quint64 address = id.section(':', 1).toULongLong(&ok, 0);
    QString html = "<h2>Instruction " + esc(id) + "</h2>";
    if (!ok) return html + "<p>Invalid address.</p>";
    const auto matches = m_model.sourceBinary().findAllByVaddr(section, address, m_model.selectedBuildId());
    html += "<p>Build: " + esc(m_model.selectedBuildId()) + " · " +
            QString::number(matches.size()) + " source correspondence(s)</p><ul>";
    for (const auto& mapping : matches) {
        html += "<li>" + esc(mapping.filePath) + ':' + QString::number(mapping.lineNumber) +
                " · revision " + esc(mapping.revision) + " · " + esc(mapping.sectionName) +
                " · <a href=\"trace:symbol:" + esc(mapping.functionName) + "\">" +
                esc(mapping.functionName) + "</a></li>";
    }
    if (matches.isEmpty()) html += "<li>No DWARF correspondence found.</li>";
    return html + "</ul>";
}

QString TraceExplorer::sourceLineCard(const QString& id) const
{
    const int colon = id.lastIndexOf(':');
    if (colon <= 0) return tr("Invalid source location");
    const QString file = id.left(colon);
    const int line = id.mid(colon + 1).toInt();
    const auto matches = m_model.sourceBinary().findAllBySourceLine(file, line, m_model.selectedBuildId());
    QString html = "<h2>Source line " + esc(id) + "</h2><p>Build " + esc(m_model.selectedBuildId()) +
                   " · " + QString::number(matches.size()) + " instruction interval(s)</p><ul>";
    for (const auto& mapping : matches) {
        const QString instruction = mapping.sectionName + ':' + hex(mapping.vaddr);
        html += "<li><a href=\"trace:instruction:" + esc(instruction) + "\">" +
                esc(instruction) + "</a> → " + esc(hex(mapping.vaddrEnd)) +
                " · " + esc(mapping.functionName) + "</li>";
    }
    if (matches.isEmpty()) html += "<li>No verified DWARF mapping for this build.</li>";
    return html + "</ul>";
}

void TraceExplorer::showDetails()
{
    QString html = "<p><small>" + esc(m_model.document().value("analysisMode").toString()) +
                   "<br>" + esc(m_backendStatus) + "</small></p>";
    if (!m_model.evidenceErrors().isEmpty())
        html += "<p style='color:#cc4444'>Evidence errors: " + esc(m_model.evidenceErrors().join("; ")) + "</p>";
    if (m_selectedKind == "requirement") html += requirementCard(m_selectedId);
    else if (m_selectedKind == "symbol") html += symbolCard(m_selectedId);
    else if (m_selectedKind == "instruction") html += instructionCard(m_selectedId);
    else if (m_selectedKind == "source-line") html += sourceLineCard(m_selectedId);
    m_details->setHtml(html);
}

void TraceExplorer::verifyBackend()
{
    if (!m_model.isSelectedBuildVerified()) {
        m_backendStatus = "Evidence mismatch; backend verification skipped";
        return;
    }
    const auto build = m_model.selectedBuild();
    const QString binary = QDir(m_model.root()).filePath(build.value("binary").toString());
    DisassemblerWorker worker;
    QSet<QString> parsed;
    QString error;
    connect(&worker, &DisassemblerWorker::sectionFound, &worker, [&parsed](const DisasmSection& section) {
        for (const auto& instruction : section.instructions) {
            bool ok = false;
            const quint64 address = instruction.address.toULongLong(&ok, 16);
            if (ok && instruction.size > 0) parsed.insert(section.name + ':' + hex(address));
        }
    });
    connect(&worker, &DisassemblerWorker::errorOccurred, &worker, [&error](const QString& message) {
        error = message;
    });
    worker.disassemble(binary, {});
    if (!error.isEmpty()) {
        m_backendStatus = "Disassembler backend error: " + error;
        return;
    }
    int missing = 0;
    for (const auto& value : build.value("ranges").toArray()) {
        const auto range = value.toObject();
        if (range.value("status").toString() != "mapped") continue;
        for (const auto& address : range.value("instructions").toArray())
            if (!parsed.contains(range.value("section").toString() + ':' + address.toString())) ++missing;
    }
    m_backendStatus = QString("DisassemblerWorker: %1 parsed instructions; %2 manifest addresses missing")
                          .arg(parsed.size()).arg(missing);
}
