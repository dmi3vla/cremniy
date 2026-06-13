#ifndef LAYER_PANEL_H
#define LAYER_PANEL_H

#include <QWidget>
#include <QCheckBox>
#include <QSettings>

class LayerPanel : public QWidget
{
    Q_OBJECT

public:
    explicit LayerPanel(QWidget* parent = nullptr);

    bool isIncludeLayerOn() const { return m_includeCheck->isChecked(); }
    bool isCallLayerOn() const { return m_callCheck->isChecked(); }
    bool isGitLayerOn() const { return m_gitCheck->isChecked(); }

    void saveState();
    void restoreState();

signals:
    void layerToggled();

private:
    QCheckBox* m_includeCheck;
    QCheckBox* m_callCheck;
    QCheckBox* m_gitCheck;
};

#endif // LAYER_PANEL_H
