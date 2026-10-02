#ifndef TRACE_EXPLORER_H
#define TRACE_EXPLORER_H

#include "trace_model.h"
#include <QDialog>
#include <QMap>

class QTreeWidget;
class QTreeWidgetItem;
class QGraphicsView;
class QGraphicsScene;
class QGraphicsRectItem;
class QTextBrowser;
class QComboBox;
class QCheckBox;
class QSpinBox;
class QLabel;

class TraceExplorer : public QDialog
{
public:
    explicit TraceExplorer(QWidget* parent = nullptr);
    bool openManifest(const QString& path, QString* error = nullptr);
    bool selectNode(const QString& kind, const QString& id);
    QString selectedNodeId() const { return m_selectedId; }
    QString detailsText() const;
    int graphNodeCount() const;

private:
    void rebuildTree();
    void rebuildGraph();
    void showDetails();
    void verifyBackend();
    void choose(const QString& kind, const QString& id);
    QGraphicsRectItem* addGraphNode(const QString& kind, const QString& id,
                                    const QString& label, const QPointF& position,
                                    const QString& groupId = {}, bool repeated = false,
                                    const QString& gitState = {});
    QString symbolCard(const QString& id) const;
    QString requirementCard(const QString& id) const;
    QString instructionCard(const QString& id) const;
    QString sourceLineCard(const QString& id) const;

    TraceModel m_model;
    QTreeWidget* m_tree = nullptr;
    QGraphicsView* m_graph = nullptr;
    QGraphicsScene* m_scene = nullptr;
    QTextBrowser* m_details = nullptr;
    QComboBox* m_build = nullptr;
    QComboBox* m_group = nullptr;
    QComboBox* m_requirement = nullptr;
    QComboBox* m_direction = nullptr;
    QCheckBox* m_changed = nullptr;
    QSpinBox* m_depth = nullptr;
    QLabel* m_legend = nullptr;
    QString m_selectedKind;
    QString m_selectedId;
    QString m_backendStatus;
    bool m_rendering = false;
    QMap<QString, QTreeWidgetItem*> m_treeItems;
};

#endif
