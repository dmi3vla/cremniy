#include "canvastab.h"
#include "nodes/file_node.h"
#include "edges/dependency_edge.h"
#include "core/ToolTabFactory.h"
#include <QToolButton>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QDir>
#include <QtMath>

static bool registered = [](){
    ToolTabFactory::instance().registerTab("4", [](FileDataBuffer* buffer){
        return new CanvasTab(buffer);
    });
    return true;
}();

CanvasTab::CanvasTab(FileDataBuffer* buffer, QWidget* parent)
    : ToolTab(buffer, parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_canvasView = new CanvasView(this);

    // Toolbar
    auto* toolbar = new QWidget(this);
    toolbar->setFixedHeight(36);
    toolbar->setStyleSheet("background: #252530;");
    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(8, 0, 8, 0);
    toolbarLayout->setSpacing(4);

    auto* zoomInBtn = createToolButton("+", "Zoom In");
    auto* zoomOutBtn = createToolButton("-", "Zoom Out");
    auto* resetBtn = createToolButton("Reset", "Reset Zoom");
    auto* refreshBtn = createToolButton("Refresh", "Refresh Graph");

    connect(zoomInBtn, &QToolButton::clicked, m_canvasView, &CanvasView::zoomIn);
    connect(zoomOutBtn, &QToolButton::clicked, m_canvasView, &CanvasView::zoomOut);
    connect(resetBtn, &QToolButton::clicked, m_canvasView, &CanvasView::resetZoom);
    connect(refreshBtn, &QToolButton::clicked, this, [this]() {
        if (m_parser && !m_projectPath.isEmpty()) {
            clearCanvas();
            m_parser->startParsing();
        }
    });

    toolbarLayout->addWidget(zoomInBtn);
    toolbarLayout->addWidget(zoomOutBtn);
    toolbarLayout->addWidget(resetBtn);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(refreshBtn);

    layout->addWidget(toolbar);
    layout->addWidget(m_canvasView);

    m_parser = nullptr;
}

void CanvasTab::setFile(QString filepath)
{
    m_projectPath = QFileInfo(filepath).absolutePath();
    m_parser = new DependencyParser(m_projectPath, this);
    connect(m_parser, &DependencyParser::graphReady, this, &CanvasTab::onGraphReady);
}

void CanvasTab::setTabData()
{
    if (!m_parser || m_projectPath.isEmpty())
        return;

    clearCanvas();
    m_parser->startParsing();
}

void CanvasTab::saveTabData()
{
    // Canvas is read-only visualization
}

void CanvasTab::onGraphReady(DependencyGraph graph)
{
    buildGraph(graph);
    layoutNodesRadial(graph);
}

void CanvasTab::buildGraph(const DependencyGraph& graph)
{
    clearCanvas();

    for (const QString& file : graph.allFiles) {
        auto* node = new FileNode(file);
        connect(node, &FileNode::clicked, this, &CanvasTab::onNodeClicked);
        connect(node, &FileNode::doubleClicked, this, &CanvasTab::onNodeDoubleClicked);
        m_canvasView->scene()->addItem(node);
        m_nodes[file] = node;
    }

    for (auto it = graph.includes.begin(); it != graph.includes.end(); ++it) {
        const QString& from = it->first;
        const QStringList& toList = it->second;

        if (!m_nodes.contains(from)) continue;

        for (const QString& to : toList) {
            if (!m_nodes.contains(to)) continue;

            auto* edge = new DependencyEdge(m_nodes[from], m_nodes[to], DependencyEdge::Include);
            m_canvasView->scene()->addItem(edge);
            m_edges.append(edge);

            m_nodes[from]->setDependencyCount(m_nodes[from]->dependencyCount() + 1);
        }
    }
}

void CanvasTab::layoutNodesRadial(const DependencyGraph& graph)
{
    if (graph.allFiles.isEmpty())
        return;

    int nodeCount = graph.allFiles.size();
    qreal radius = qSqrt(nodeCount) * 80;
    qreal angleStep = 2 * M_PI / nodeCount;

    for (int i = 0; i < graph.allFiles.size(); ++i) {
        const QString& file = graph.allFiles[i];
        if (!m_nodes.contains(file)) continue;

        qreal angle = i * angleStep;
        qreal x = radius * qCos(angle);
        qreal y = radius * qSin(angle);
        m_nodes[file]->setPos(x, y);
    }

    for (auto* edge : m_edges)
        edge->updatePosition();
}

void CanvasTab::clearCanvas()
{
    qDeleteAll(m_edges);
    m_edges.clear();
    qDeleteAll(m_nodes.values());
    m_nodes.clear();
}

void CanvasTab::highlightActiveFile(const QString& filePath)
{
    for (auto* node : m_nodes)
        node->setState(FileNode::Normal);

    if (m_nodes.contains(filePath)) {
        m_nodes[filePath]->setState(FileNode::Active);
        m_nodes[filePath]->startPulse();
    }
}

void CanvasTab::highlightDependencies(const QString& filePath)
{
    for (auto* edge : m_edges) {
        bool isRelated = edge->source()->filePath() == filePath ||
                         edge->target()->filePath() == filePath;
        edge->setVisible(isRelated || filePath.isEmpty());
    }

    for (auto* node : m_nodes) {
        bool isRelated = node->filePath() == filePath;
        if (!isRelated) {
            for (auto* edge : m_edges) {
                if ((edge->source()->filePath() == filePath && edge->target() == node) ||
                    (edge->target()->filePath() == filePath && edge->source() == node)) {
                    isRelated = true;
                    break;
                }
            }
        }
        node->setOpacity(isRelated || filePath.isEmpty() ? 1.0 : 0.3);
    }
}

void CanvasTab::onNodeClicked(const QString& filePath)
{
    highlightDependencies(filePath);
}

void CanvasTab::onNodeDoubleClicked(const QString& filePath)
{
    emit fileOpenRequested(filePath);
}

QToolButton* CanvasTab::createToolButton(const QString& text, const QString& tooltip)
{
    auto* btn = new QToolButton(this);
    btn->setText(text);
    btn->setToolTip(tooltip);
    btn->setStyleSheet(
        "QToolButton { color: #ccc; background: #3a3a4a; border: 1px solid #555; "
        "padding: 4px 8px; border-radius: 4px; }"
        "QToolButton:hover { background: #4a4a5a; }"
    );
    return btn;
}
