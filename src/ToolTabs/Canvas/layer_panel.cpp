#include "layer_panel.h"
#include <QVBoxLayout>
#include <QLabel>

LayerPanel::LayerPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);

    auto* label = new QLabel("Layers", this);
    label->setStyleSheet("color: #aaa; font-weight: bold; font-size: 10px;");
    layout->addWidget(label);

    m_includeCheck = new QCheckBox("#include", this);
    m_includeCheck->setChecked(true);
    m_includeCheck->setStyleSheet("color: #ccc; font-size: 10px;");

    m_callCheck = new QCheckBox("function calls", this);
    m_callCheck->setChecked(true);
    m_callCheck->setStyleSheet("color: #ccc; font-size: 10px;");

    m_gitCheck = new QCheckBox("git history", this);
    m_gitCheck->setChecked(false);
    m_gitCheck->setStyleSheet("color: #ccc; font-size: 10px;");

    layout->addWidget(m_includeCheck);
    layout->addWidget(m_callCheck);
    layout->addWidget(m_gitCheck);

    setStyleSheet("background: #2a2a35; border: 1px solid #444; border-radius: 4px;");

    connect(m_includeCheck, &QCheckBox::toggled, this, &LayerPanel::layerToggled);
    connect(m_callCheck, &QCheckBox::toggled, this, &LayerPanel::layerToggled);
    connect(m_gitCheck, &QCheckBox::toggled, this, &LayerPanel::layerToggled);

    restoreState();
}

void LayerPanel::saveState()
{
    QSettings settings;
    settings.beginGroup("Canvas/Layers");
    settings.setValue("include", m_includeCheck->isChecked());
    settings.setValue("calls", m_callCheck->isChecked());
    settings.setValue("git", m_gitCheck->isChecked());
    settings.endGroup();
}

void LayerPanel::restoreState()
{
    QSettings settings;
    settings.beginGroup("Canvas/Layers");
    m_includeCheck->setChecked(settings.value("include", true).toBool());
    m_callCheck->setChecked(settings.value("calls", true).toBool());
    m_gitCheck->setChecked(settings.value("git", false).toBool());
    settings.endGroup();
}
