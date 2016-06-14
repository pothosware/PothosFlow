// Copyright (c) 2013-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "MainWindow/MainWindow.hpp"
#include <Pothos/Util/Network.hpp>
#include <Pothos/System.hpp>
#include <Pothos/Init.hpp>
#include "BlockTree/BlockCache.hpp"
#include "BlockTree/BlockTreeDock.hpp"
#include "PropertiesPanel/PropertiesPanelDock.hpp"
#include "GraphEditor/GraphEditorTabs.hpp"
#include "GraphEditor/GraphActionsDock.hpp"
#include "HostExplorer/HostExplorerDock.hpp"
#include "AffinitySupport/AffinityZonesDock.hpp"
#include "MessageWindow/MessageWindowDock.hpp"
#include "ColorUtils/ColorsDialog.hpp"
#include "MainWindow/MainActions.hpp"
#include "MainWindow/MainMenu.hpp"
#include "MainWindow/MainToolBar.hpp"
#include "MainWindow/MainSettings.hpp"
#include "MainWindow/MainSplash.hpp"
#include <QMainWindow>
#include <QGridLayout>
#include <QSettings>
#include <QDockWidget>
#include <QMenuBar>
#include <QToolBar>
#include <QAction>
#include <QTabWidget>
#include <QMessageBox>
#include <QMap>
#include <Poco/Logger.h>
#include <iostream>

PothosGuiMainWindow::PothosGuiMainWindow(QWidget *parent):
    QMainWindow(parent),
    _splash(new PothosGuiMainSplash(this)),
    _settings(new PothosGuiMainSettings(this)),
    _actions(nullptr)
{
    _splash->show();
    _splash->postMessage(tr("Creating main window..."));

    //try to talk to the server on localhost, if not there, spawn a custom one
    //make a server and node that is temporary with this process
    _splash->postMessage(tr("Launching scratch process..."));
    try
    {
        Pothos::RemoteClient client("tcp://"+Pothos::Util::getLoopbackAddr());
    }
    catch (const Pothos::RemoteClientError &)
    {
        _server = Pothos::RemoteServer("tcp://"+Pothos::Util::getLoopbackAddr(Pothos::RemoteServer::getLocatorPort()));
        //TODO make server background so it does not close with process
        Pothos::RemoteClient client("tcp://"+Pothos::Util::getLoopbackAddr()); //now it should connect to the new server
    }

    _splash->postMessage(tr("Initializing Pothos plugins..."));
    Pothos::init();

    #ifdef __APPLE__
    //enable the menu bar - since its not showing up as the global menu
    this->menuBar()->setNativeMenuBar(false);
    //enable the title bar - otherwise its invisible
    this->setUnifiedTitleAndToolBarOnMac(true);
    #endif //__APPLE__

    this->setMinimumSize(800, 600);
    this->setWindowTitle("Pothos GUI");

    //initialize actions and action buttons
    _splash->postMessage(tr("Creating actions..."));
    _actions = new PothosGuiMainActions(this);
    _splash->postMessage(tr("Creating toolbar..."));
    auto mainToolBar = new PothosGuiMainToolBar(this, _actions);
    _splash->postMessage(tr("Creating menus..."));
    auto mainMenu = new PothosGuiMainMenu(this, _actions);

    //create message window dock
    _splash->postMessage(tr("Creating message window..."));
    auto messageWindowDock = new MessageWindowDock(this);
    this->addDockWidget(Qt::BottomDockWidgetArea, messageWindowDock);
    poco_information_f1(Poco::Logger::get("PothosGui.MainWindow"), "Welcome to Pothos v%s", Pothos::System::getApiVersion());

    //create graph actions dock
    _splash->postMessage(tr("Creating actions dock..."));
    auto graphActionsDock = new GraphActionsDock(this);
    this->addDockWidget(Qt::BottomDockWidgetArea, graphActionsDock);

    //create host explorer dock
    _splash->postMessage(tr("Creating host explorer..."));
    auto hostExplorerDock = new HostExplorerDock(this);
    this->addDockWidget(Qt::RightDockWidgetArea, hostExplorerDock);

    //create affinity panel
    _splash->postMessage(tr("Creating affinity panel..."));
    auto affinityZonesDock = new AffinityZonesDock(this, hostExplorerDock);
    this->tabifyDockWidget(hostExplorerDock, affinityZonesDock);
    auto editMenu = mainMenu->editMenu;
    mainMenu->affinityZoneMenu = AffinityZonesDock::global()->makeMenu(editMenu);
    editMenu->addMenu(mainMenu->affinityZoneMenu);

    //block cache (make before block tree)
    _splash->postMessage(tr("Creating block cache..."));
    auto blockCache = new BlockCache(this, hostExplorerDock);
    connect(this, SIGNAL(initDone(void)), blockCache, SLOT(handleUpdate(void)));

    //create topology editor tabbed widget
    _splash->postMessage(tr("Creating graph editor..."));
    auto editorTabs = new GraphEditorTabs(this);
    this->setCentralWidget(editorTabs);
    connect(this, SIGNAL(initDone(void)), editorTabs, SLOT(handleInit(void)));
    connect(this, SIGNAL(exitBegin(QCloseEvent *)), editorTabs, SLOT(handleExit(QCloseEvent *)));

    //create block tree (after the block cache)
    _splash->postMessage(tr("Creating block tree..."));
    auto blockTreeDock = new BlockTreeDock(this, blockCache, editorTabs);
    connect(_actions->findAction, SIGNAL(triggered(void)), blockTreeDock, SLOT(activateFind(void)));
    this->tabifyDockWidget(affinityZonesDock, blockTreeDock);

    //create properties panel (make after block cache)
    _splash->postMessage(tr("Creating properties panel..."));
    auto propertiesPanelDock = new PropertiesPanelDock(this);
    this->tabifyDockWidget(blockTreeDock, propertiesPanelDock);

    //restore main window settings from file
    _splash->postMessage(tr("Restoring configuration..."));
    this->restoreGeometry(_settings->value("MainWindow/geometry").toByteArray());
    this->restoreState(_settings->value("MainWindow/state").toByteArray());
    propertiesPanelDock->hide(); //hidden until used
    _actions->showPortNamesAction->setChecked(_settings->value("MainWindow/showPortNames", true).toBool());
    _actions->eventPortsInlineAction->setChecked(_settings->value("MainWindow/eventPortsInline", true).toBool());
    _actions->clickConnectModeAction->setChecked(_settings->value("MainWindow/clickConnectMode", false).toBool());
    _actions->showGraphConnectionPointsAction->setChecked(_settings->value("MainWindow/showGraphConnectionPoints", false).toBool());
    _actions->showGraphBoundingBoxesAction->setChecked(_settings->value("MainWindow/showGraphBoundingBoxes", false).toBool());

    //finish view menu after docks and tool bars (view menu calls their toggleViewAction())
    auto viewMenu = mainMenu->viewMenu;
    viewMenu->addAction(hostExplorerDock->toggleViewAction());
    viewMenu->addAction(messageWindowDock->toggleViewAction());
    viewMenu->addAction(graphActionsDock->toggleViewAction());
    viewMenu->addAction(blockTreeDock->toggleViewAction());
    viewMenu->addAction(affinityZonesDock->toggleViewAction());
    viewMenu->addAction(mainToolBar->toggleViewAction());

    //setup is complete, show the window and signal done
    this->show();
    connect(this, SIGNAL(initDone(void)), this, SLOT(handleInitDone(void)));
    emit this->initDone();
}

PothosGuiMainWindow::~PothosGuiMainWindow(void)
{
    this->handleFullScreenViewAction(false); //undo if set -- so we dont save full mode below
    _settings->setValue("MainWindow/geometry", this->saveGeometry());
    _settings->setValue("MainWindow/state", this->saveState());
    _settings->setValue("MainWindow/showPortNames", _actions->showPortNamesAction->isChecked());
    _settings->setValue("MainWindow/eventPortsInline", _actions->eventPortsInlineAction->isChecked());
    _settings->setValue("MainWindow/clickConnectMode", _actions->clickConnectModeAction->isChecked());
    _settings->setValue("MainWindow/showGraphConnectionPoints", _actions->showGraphConnectionPointsAction->isChecked());
    _settings->setValue("MainWindow/showGraphBoundingBoxes", _actions->showGraphBoundingBoxesAction->isChecked());

    //cleanup all widgets which may use plugins or the server
    for (auto obj : this->children()) delete obj;

    //unload the plugins
    Pothos::deinit();

    //stop the server
    _server = Pothos::RemoteServer();
}

void PothosGuiMainWindow::handleInitDone(void)
{
    _splash->postMessage(tr("Completing initialization..."));
    _splash->finish(this);
    poco_information(Poco::Logger::get("PothosGui.MainWindow"), "Initialization complete");
}

void PothosGuiMainWindow::handleNewTitleSubtext(const QString &s)
{
    this->setWindowTitle("Pothos GUI - " + s);
}

void PothosGuiMainWindow::handleShowAbout(void)
{
    QMessageBox::about(this, "About Pothos", QString(
        "Pothos v%1\n"
        "Install %2\n"
        "www.pothosware.com")
        .arg(QString::fromStdString(Pothos::System::getApiVersion()))
        .arg(QString::fromStdString(Pothos::System::getRootPath())));
}

void PothosGuiMainWindow::handleShowAboutQt(void)
{
    QMessageBox::aboutQt(this);
}

void PothosGuiMainWindow::handleColorsDialogAction(void)
{
    auto dialog = new ColorsDialog(this);
    dialog->exec();
    delete dialog;
}

void PothosGuiMainWindow::handleFullScreenViewAction(const bool toggle)
{
    //gather a list of widgets to show/hide
    if (toggle and _widgetToOldVisibility.empty())
    {
        for (auto child : this->children())
        {
            auto dockWidget = dynamic_cast<QDockWidget *>(child);
            if (dockWidget != nullptr) _widgetToOldVisibility[dockWidget];
        }
        _widgetToOldVisibility[this->menuBar()];
    }

    //save state on all widgets and then hide
    if (toggle) for (auto &pair : _widgetToOldVisibility)
    {
        pair.second = pair.first->isVisible();
        pair.first->hide();
    }

    //restore state on all widgets
    else for (const auto &pair : _widgetToOldVisibility)
    {
        pair.first->setVisible(pair.second);
    }
}

void PothosGuiMainWindow::closeEvent(QCloseEvent *event)
{
    emit this->exitBegin(event);
}

void PothosGuiMainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
}
