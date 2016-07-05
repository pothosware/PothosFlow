// Copyright (c) 2013-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Config.hpp>
#include <QObject>

class QMenu;
class QMainWindow;
class MainActions;

/*!
 * This class contains top level QActions used in the menu
 * and connected to slots throughout the application.
 */
class MainMenu : public QObject
{
    Q_OBJECT
public:

    //! Global access to main menus
    static MainMenu *global(void);

    MainMenu(QMainWindow *parent, MainActions *actions);

    QMenu *fileMenu;
    QMenu *exportMenu;
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
