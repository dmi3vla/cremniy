#ifndef TOOLSMENU_H
#define TOOLSMENU_H

#include "ui/MenuBar/basemenu.h"

class IDEWindow;

class ToolsMenu : public BaseMenu
{
    Q_OBJECT
private:
    QAction* m_reverseCalculator;
    QAction* m_generateSemanticMap;
    IDEWindow* m_ideWindow = nullptr;
public:
    ToolsMenu();
    void setupConnections(IDEWindow* ideWind);
private:
    void on_Open_ReverseCalculator();
    void on_GenerateSemanticMap();
};

#endif // TOOLSMENU_H
