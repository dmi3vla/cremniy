#ifndef CANVASTAB_H
#define CANVASTAB_H

#include "core/ToolTab.h"
#include "canvas_view.h"
#include "canvas_layout.h"
#include "dependency_parser.h"
#include <QVBoxLayout>
#include <QToolButton>
#include <QLabel>

class FileNode;
class DependencyEdge;

class CanvasTab : public ToolTab
{
    Q_OBJECT

public:
    explicit CanvasTab(FileDataBuffer* buffer, QWidget* parent = nullptr);

    QString toolName() const override { return "Canvas"; }
    QIcon toolIcon() const override { return QIcon(":/icons/canvas.png"); }

public slots:
    void setFile(QString filepath) override;
    void setTabData() override;
    void saveTabData() override;

    void highlightActiveFile(const QString& filePath);
    void highlightDependencies(const QString& filePath);

private slots:
    void onGraphReady(DependencyGraph graph);
    void onNodeClicked(const QString& filePath);
    void onNodeDoubleClicked(const QString& filePath);

private:
    CanvasView* m_canvasView;
    DependencyParser* m_parser;
    CanvasLayout* m_layout;
    QString m_projectPath;

    QMap<QString, FileNode*> m_nodes;
    QList<DependencyEdge*> m_edges;
    DependencyGraph m_currentGraph;

    void buildGraph(const DependencyGraph& graph);
    void layoutNodesRadial(const DependencyGraph& graph);
    void clearCanvas();
    QToolButton* createToolButton(const QString& text, const QString& tooltip);
};

#endif // CANVASTAB_H
