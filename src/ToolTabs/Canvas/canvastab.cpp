#include "canvastab.h"
#include "nodes/file_node.h"
#include "edges/dependency_edge.h"
#include "core/ToolTabFactory.h"
#include <QToolButton>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QDir>
#include <QtMath>
#include <QSlider>
#include <QResizeEvent>
#include <QTimer>
#include <QSet>

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
    m_layout = new CanvasLayout(this);

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

    // Gource controls
    auto* playBtn = createToolButton("Play", "Play git history");
    auto* pauseBtn = createToolButton("Pause", "Pause playback");
    auto* speedSlider = new QSlider(Qt::Horizontal, this);
    speedSlider->setRange(1, 10);
    speedSlider->setValue(2);
    speedSlider->setFixedWidth(80);
    speedSlider->setToolTip("Playback speed");

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
    toolbarLayout->addWidget(playBtn);
    toolbarLayout->addWidget(pauseBtn);
    toolbarLayout->addWidget(speedSlider);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(refreshBtn);

    layout->addWidget(toolbar);
    layout->addWidget(m_canvasView);

    // Layer panel (overlay top-right)
    m_layerPanel = new LayerPanel(this);
    m_layerPanel->setFixedSize(120, 90);
    m_layerPanel->move(width() - 130, 40);
    m_layerPanel->raise();

    // Minimap (overlay bottom-right)
    m_minimap = new Minimap(m_canvasView, this);
    m_minimap->move(width() - 170, height() - 130);
    m_minimap->raise();

    // Gource animator
    m_animator = nullptr;
    m_parser = nullptr;

    connect(playBtn, &QToolButton::clicked, this, [this]() {
        if (m_animator) m_animator->play();
    });
    connect(pauseBtn, &QToolButton::clicked, this, [this]() {
        if (m_animator) m_animator->pause();
    });
    connect(speedSlider, &QSlider::valueChanged, this, [this](int val) {
        if (m_animator) m_animator->setSpeed(val);
    });
    connect(m_layerPanel, &LayerPanel::layerToggled, this, &CanvasTab::applyLayerFilters);
}

void CanvasTab::setFile(QString filepath)
{
    m_projectPath = QFileInfo(filepath).absolutePath();
    m_parser = new DependencyParser(m_projectPath, this);
    connect(m_parser, &DependencyParser::graphReady, this, &CanvasTab::onGraphReady);
    connect(m_parser, &DependencyParser::graphUpdated, this, [this](DependencyGraph graph) {
        diffGraphUpdate(m_currentGraph, graph);
    });
    m_parser->watchForChanges();

    // Gource animator
    m_animator = new GourceAnimator(m_projectPath, this);
    connect(m_animator, &GourceAnimator::commitReady, this, &CanvasTab::onGourceCommit);
    m_animator->loadHistory();
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
    m_currentGraph = graph;
    buildGraph(graph);
    layoutNodesRadial(graph);
}

void CanvasTab::buildGraph(const DependencyGraph& graph)
{
    clearCanvas();

    // Build incoming deps map
    QMap<QString, QStringList> incomingDeps;
    for (auto it = graph.includes.begin(); it != graph.includes.end(); ++it) {
        for (const QString& to : it->second) {
            incomingDeps[to].append(it->first);
        }
    }

    for (const QString& file : graph.allFiles) {
        auto* node = new FileNode(file);
        connectNodeSignals(node);

        // Set dependency info for tooltip
        auto incIt = graph.includes.find(file);
        if (incIt != graph.includes.end())
            node->setOutgoingDeps(incIt->second);
        if (incomingDeps.contains(file))
            node->setIncomingDeps(incomingDeps[file]);

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

void CanvasTab::connectNodeSignals(FileNode* node)
{
    connect(node, &FileNode::clicked, this, &CanvasTab::onNodeClicked);
    connect(node, &FileNode::doubleClicked, this, &CanvasTab::onNodeDoubleClicked);
    connect(node, &FileNode::openInEditor, this, [this](const QString& path) {
        emit fileOpenRequested(path);
    });
    connect(node, &FileNode::openInHexEditor, this, [this](const QString& path) {
        emit fileOpenRequested(path);
    });
    connect(node, &FileNode::hideUnconnected, this, &CanvasTab::highlightDependencies);
}

void CanvasTab::layoutNodesRadial(const DependencyGraph& graph)
{
    if (graph.allFiles.isEmpty())
        return;

    QMap<QString, QPointF> targets;
    m_layout->computeTargets(graph, targets);
    m_layout->animateNodesToPositions(targets, m_nodes, 400);
}

QPointF CanvasTab::computePositionForNewNode(const QString& path)
{
    QString dir = QFileInfo(path).absolutePath();

    // Find nearest existing file in same directory
    for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it) {
        if (QFileInfo(it.key()).absolutePath() == dir) {
            return it.value()->pos() + QPointF(40, 30);
        }
    }

    // Fallback: place near center with offset based on file count
    qreal offset = m_nodes.size() * 15.0;
    return QPointF(offset - (m_nodes.size() * 7.5), offset * 0.5);
}

void CanvasTab::diffGraphUpdate(const DependencyGraph& oldGraph, const DependencyGraph& newGraph)
{
    QSet<QString> oldFiles(oldGraph.allFiles.begin(), oldGraph.allFiles.end());
    QSet<QString> newFiles(newGraph.allFiles.begin(), newGraph.allFiles.end());

    QSet<QString> added = newFiles - oldFiles;
    QSet<QString> removed = oldFiles - newFiles;

    // 1) Remove deleted files — animate edges first, then node
    for (const QString& file : removed) {
        for (int i = m_edges.size() - 1; i >= 0; --i) {
            DependencyEdge* edge = m_edges[i];
            if (edge->source()->filePath() == file || edge->target()->filePath() == file) {
                edge->stopAnimations();
                edge->playDisappearAnimation();
                connect(edge, &DependencyEdge::disappearFinished, this, [this, edge]() {
                    m_canvasView->scene()->removeItem(edge);
                    m_edges.removeOne(edge);
                    edge->deleteLater();
                });
            }
        }
        if (m_nodes.contains(file)) {
            FileNode* node = m_nodes[file];
            node->playDisappearAnimation();
            connect(node, &FileNode::disappearFinished, this, [this, node, file]() {
                m_canvasView->scene()->removeItem(node);
                m_nodes.remove(file);
                node->deleteLater();
            });
        }
    }

    // 2) Add new files
    QMap<QString, QStringList> newIncomingDeps;
    for (auto it = newGraph.includes.begin(); it != newGraph.includes.end(); ++it) {
        for (const QString& to : it->second) {
            newIncomingDeps[to].append(it->first);
        }
    }

    for (const QString& file : added) {
        auto* node = new FileNode(file);
        connectNodeSignals(node);

        auto incIt = newGraph.includes.find(file);
        if (incIt != newGraph.includes.end())
            node->setOutgoingDeps(incIt->second);
        if (newIncomingDeps.contains(file))
            node->setIncomingDeps(newIncomingDeps[file]);

        node->setPos(computePositionForNewNode(file));
        m_canvasView->scene()->addItem(node);
        m_nodes[file] = node;
        node->playAppearAnimation();
    }

    // 3) Add new edges (for both new and unchanged files)
    for (auto it = newGraph.includes.begin(); it != newGraph.includes.end(); ++it) {
        const QString& from = it->first;
        const QStringList& toList = it->second;
        if (!m_nodes.contains(from)) continue;

        for (const QString& to : toList) {
            if (!m_nodes.contains(to)) continue;

            bool exists = false;
            for (DependencyEdge* e : m_edges) {
                if (e->source()->filePath() == from && e->target()->filePath() == to) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                auto* edge = new DependencyEdge(m_nodes[from], m_nodes[to], DependencyEdge::Include);
                m_canvasView->scene()->addItem(edge);
                m_edges.append(edge);
                edge->playAppearAnimation();
            }
        }
    }

    // 4) Update dependency counts for all remaining nodes
    for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it) {
        int count = 0;
        auto incIt = newGraph.includes.find(it.key());
        if (incIt != newGraph.includes.end())
            count = incIt->second.size();
        it.value()->setDependencyCount(count);
    }

    m_currentGraph = newGraph;
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

void CanvasTab::applyLayerFilters()
{
    bool showInclude = m_layerPanel->isIncludeLayerOn();
    bool showCall = m_layerPanel->isCallLayerOn();
    bool showGit = m_layerPanel->isGitLayerOn();

    for (auto* edge : m_edges) {
        bool visible = true;
        switch (edge->edgeType()) {
        case DependencyEdge::Include: visible = showInclude; break;
        case DependencyEdge::Call:    visible = showCall; break;
        case DependencyEdge::Inherit: visible = true; break;
        }
        edge->setVisible(visible);
    }

    Q_UNUSED(showGit)
    m_layerPanel->saveState();
}

void CanvasTab::onGourceCommit(const GitCommit& commit)
{
    for (const QString& file : commit.files) {
        QString fullPath = m_projectPath + "/" + file;
        if (m_nodes.contains(fullPath)) {
            m_nodes[fullPath]->startPulse();
        }
    }

    m_minimap->updateViewportRect();
}

void CanvasTab::resizeEvent(QResizeEvent* event)
{
    ToolTab::resizeEvent(event);
    if (m_layerPanel)
        m_layerPanel->move(width() - 130, 40);
    if (m_minimap)
        m_minimap->move(width() - 170, height() - 130);
}

void CanvasTab::startNodePulsing(const QString& filePath)
{
    stopNodePulsing();
    m_pulsingFilePath = filePath;
    if (!m_pulseTimer) {
        m_pulseTimer = new QTimer(this);
        connect(m_pulseTimer, &QTimer::timeout, this, [this]() {
            FileNode* node = m_nodes.value(m_pulsingFilePath, nullptr);
            if (node) {
                node->startPulse();
            } else {
                stopNodePulsing();
            }
        });
    }
    m_pulseTimer->start(800);
}

void CanvasTab::stopNodePulsing()
{
    if (m_pulseTimer) {
        m_pulseTimer->stop();
    }
    m_pulsingFilePath.clear();
}
