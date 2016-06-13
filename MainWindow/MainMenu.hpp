// Copyright (c) 2013-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Config.hpp>
#include <QObject>

class QMenu;
class QMainWindow;
class PothosGuiMainActions;

/*!
 * This class contains top level QActions used in the menu
 * and connected to slots throughout the application.
 */
class PothosGuiMainMenu : public QObject
{
    Q_OBJECT
public:

    //! Global access to main menus
    static PothosGuiMainMenu *global(void);

    PothosGuiMainMenu(QMainWindow *parent, PothosGuiMainActions *actions);

    QMenu *fileMenu;
    QMenu *editMenu;
    QMenu *affinityZoneMenu;
    QMenu *moveGraphObjectsMenu;
    QMenu *insertGraphWidgetsMenu;
    QMenu *executeMenu;
    QMenu *viewMenu;
    QMenu *toolsMenu;
    QMenu *debugMenu;
    QMenu *configMenu;
    QMenu *helpMenu;
};
