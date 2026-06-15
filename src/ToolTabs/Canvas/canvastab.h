#ifndef CANVASTAB_H
#define CANVASTAB_H

#include "core/ToolTab.h"
#include "canvas_view.h"
#include "canvas_layout.h"
#include "dependency_parser.h"
#include "gource_animator.h"
#include "layer_panel.h"
#include "minimap.h"
#include "codemap.h"
#include <QVBoxLayout>
#include <QToolButton>
#include <QLabel>
#include <QSlider>

class FileNode;
class DependencyEdge;
class StepNode;
class ClusterGroupNode;
class ConnectionEdge;
class DigestPanel;

class CanvasTab : public ToolTab
{
    Q_OBJECT

public:
    explicit CanvasTab(FileDataBuffer* buffer, QWidget* parent = nullptr);

    QString toolName() const override { return "Canvas"; }
    QIcon toolIcon() const override { return QIcon(":/icons/canvas.png"); }

    DependencyGraph currentGraph() const { return m_currentGraph; }

    void showCodemap(const Codemap& map);
    void showStructureGraph();

public slots:
    void setFile(QString filepath) override;
    void setTabData() override;
    void saveTabData() override;

    void highlightActiveFile(const QString& filePath);
    void highlightDependencies(const QString& filePath);
    void startNodePulsing(const QString& filePath);
    void stopNodePulsing();

    void toggleGraphMode();
    void onCodemapReady(const Codemap& map);

signals:
    void codemapShown(const Codemap& map);
    void needsSemanticMapGeneration();
    void stepNavigationRequested(const QString& filePath, int lineNumber);

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onGraphReady(DependencyGraph graph);
    void onNodeClicked(const QString& filePath);
    void onNodeDoubleClicked(const QString& filePath);
    void onStepClicked(const QString& filePath, int lineNumber);

private:
    CanvasView* m_canvasView;
    DependencyParser* m_parser;
    CanvasLayout* m_layout;
    GourceAnimator* m_animator;
    LayerPanel* m_layerPanel;
    Minimap* m_minimap;
    DigestPanel* m_digestPanel = nullptr;
    QString m_projectPath;

    QMap<QString, FileNode*> m_nodes;
    QList<DependencyEdge*> m_edges;
    DependencyGraph m_currentGraph;
    Codemap m_currentCodemap;
    LayoutMode m_layoutMode = LayoutMode::Radial;

    bool m_structuralMode = true;
    bool m_generatingConceptMap = false;

    // Semantic map nodes (separate from structural graph nodes)
    QList<ClusterGroupNode*> m_clusterNodes;
    QList<ConnectionEdge*> m_connectionEdges;

    void buildGraph(const DependencyGraph& graph);
    void layoutNodesRadial(const DependencyGraph& graph);
    void clearCanvas();
    void clearSemanticNodes();
    QToolButton* createToolButton(const QString& text, const QString& tooltip);
    void applyLayerFilters();
    void onGourceCommit(const GitCommit& commit);
    void diffGraphUpdate(const DependencyGraph& oldGraph, const DependencyGraph& newGraph);
    QPointF computePositionForNewNode(const QString& path);
    void connectNodeSignals(FileNode* node);
    void focusOnChain(const QStringList& chain);

    QTimer* m_pulseTimer = nullptr;
    QString m_pulsingFilePath;
};

#endif // CANVASTAB_H
