#include "toolsmenu.h"
#include "dialogs/reversecalculatordialog.h"
#include "ui/MenuBar/menufactory.h"
#include "app/IDEWindow/idewindow.h"
#include "Agent/agent_session.h"
#include "Agent/tools/tool_registry.h"
#include "Agent/tools/agent_tool.h"
#include "ToolTabs/Canvas/canvastab.h"
#include "ToolTabs/Canvas/codemap.h"
#include "ToolTabs/Canvas/codemap_store.h"
#include <QKeySequence>
#include <QStatusBar>
#include <QJsonDocument>
#include <QJsonArray>

static bool registered = []() {
  MenuFactory::instance().registerMenu("5", []() { return new ToolsMenu(); });
  return true;
}();

ToolsMenu::ToolsMenu() : BaseMenu("Tools") {
  m_reverseCalculator = new QAction("Reverse Calculator", this);
  m_reverseCalculator->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));
  this->addAction(m_reverseCalculator);

  this->addSeparator();

  m_generateCodemap = new QAction("Generate Codemap", this);
  this->addAction(m_generateCodemap);
}

void ToolsMenu::setupConnections(IDEWindow *ideWind) {
  m_ideWindow = ideWind;
  connect(m_reverseCalculator, &QAction::triggered, this,
          &ToolsMenu::on_Open_ReverseCalculator);
  connect(m_generateCodemap, &QAction::triggered, this,
          &ToolsMenu::on_GenerateSemanticMap);
}

void ToolsMenu::on_Open_ReverseCalculator() {
  auto *dlg = new ReverseCalculatorDialog(m_ideWindow);
  dlg->setAttribute(Qt::WA_DeleteOnClose, true);
  if (m_ideWindow) {
    dlg->adjustSize();
    dlg->move(m_ideWindow->geometry().center() - dlg->rect().center());
  }
  dlg->show();
  dlg->raise();
  dlg->activateWindow();
}

void ToolsMenu::on_GenerateSemanticMap() {
  if (m_ideWindow) {
    m_ideWindow->openOrGenerateConceptMap();
  }
}
