// Copyright (c) 2013-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "MainWindow/MainWindow.hpp"
#include <Pothos/Util/Network.hpp>
#include <Pothos/System.hpp>
#include <Pothos/Init.hpp>
#include "BlockTree/BlockCache.hpp"
#include "BlockTree/BlockTreeDock.hpp"
#include "PropertiesPanel/PropertiesPanelDock.hpp"
#include "GraphEditor/GraphEditor.hpp"
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
#include <QMenuBar>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent):
    QMainWindow(parent),
    _logger(Poco::Logger::get("PothosGui.MainWindow")),
    _splash(new MainSplash(this)),
    _settings(new MainSettings(this)),
    _actions(nullptr),
    _blockCache(nullptr),
    _editorTabs(nullptr),
    _propertiesPanel(nullptr)
{
    _splash->show();
    _splash->postMessage(tr("Creating main window..."));

    _splash->postMessage(tr("Launching scratch process..."));
    this->setupServer();

    _splash->postMessage(tr("Loading Pothos plugins..."));
    Pothos::init();

    this->setMinimumSize(800, 600);
    this->setWindowTitle("Pothos GUI");

    //initialize actions and action buttons
    _splash->postMessage(tr("Creating actions..."));
    _actions = new MainActions(this);
    _splash->postMessage(tr("Creating toolbar..."));
    auto mainToolBar = new MainToolBar(this, _actions);
    this->addToolBar(mainToolBar);
    _splash->postMessage(tr("Creating menus..."));
    auto mainMenu = new MainMenu(this, _actions);

    //connect actions to the main window
    connect(_actions->exitAction, SIGNAL(triggered(void)), this, SLOT(close(void)));
    connect(_actions->showAboutAction, SIGNAL(triggered(void)), this, SLOT(handleShowAbout(void)));
    connect(_actions->showAboutQtAction, SIGNAL(triggered(void)), this, SLOT(handleShowAboutQt(void)));
    connect(_actions->showColorsDialogAction, SIGNAL(triggered(void)), this, SLOT(handleColorsDialogAction(void)));
    connect(_actions->fullScreenViewAction, SIGNAL(toggled(bool)), this, SLOT(handleFullScreenViewAction(bool)));
    connect(_actions->reloadPluginsAction, SIGNAL(triggered(bool)), this, SLOT(handleReloadPlugins(void)));

    //create message window dock
    _splash->postMessage(tr("Creating message window..."));
    auto messageWindowDock = new MessageWindowDock(this);
    this->addDockWidget(Qt::BottomDockWidgetArea, messageWindowDock);
    poco_information_f1(_logger, "Welcome to Pothos v%s", Pothos::System::getApiVersion());

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
    mainMenu->affinityZoneMenu = affinityZonesDock->makeMenu(editMenu);
    editMenu->addMenu(mainMenu->affinityZoneMenu);

    //block cache (make before block tree)
    _splash->postMessage(tr("Creating block cache..."));
    _blockCache = new BlockCache(this, hostExplorerDock);
    connect(this, SIGNAL(initDone(void)), _blockCache, SLOT(update(void)));

    //create topology editor tabbed widget
    _splash->postMessage(tr("Creating graph editor..."));
    _editorTabs = new GraphEditorTabs(this);
    this->setCentralWidget(_editorTabs);
    connect(this, SIGNAL(initDone(void)), _editorTabs, SLOT(loadState(void)));
    connect(this, SIGNAL(exitBegin(QCloseEvent *)), _editorTabs, SLOT(handleExit(QCloseEvent *)));

    //create block tree (after the block cache)
    _splash->postMessage(tr("Creating block tree..."));
    auto blockTreeDock = new BlockTreeDock(this, _blockCache, _editorTabs);
    connect(_actions->findAction, SIGNAL(triggered(void)), blockTreeDock, SLOT(activateFind(void)));
    this->tabifyDockWidget(affinityZonesDock, blockTreeDock);

    //create properties panel (make after block cache)
    _splash->postMessage(tr("Creating properties panel..."));
    _propertiesPanel = new PropertiesPanelDock(this);
    this->tabifyDockWidget(blockTreeDock, _propertiesPanel);

    //restore main window settings from file
    _splash->postMessage(tr("Restoring configuration..."));
    this->restoreGeometry(_settings->value("MainWindow/geometry").toByteArray());
    this->restoreState(_settings->value("MainWindow/state").toByteArray());
    _propertiesPanel->hide(); //hidden until used
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

MainWindow::~MainWindow(void)
{
    poco_information(_logger, "Save application state");
    this->handleFullScreenViewAction(false); //undo if set -- so we dont save full mode below
    _settings->setValue("MainWindow/geometry", this->saveGeometry());
    _settings->setValue("MainWindow/state", this->saveState());
    _settings->setValue("MainWindow/showPortNames", _actions->showPortNamesAction->isChecked());
    _settings->setValue("MainWindow/eventPortsInline", _actions->eventPortsInlineAction->isChecked());
    _settings->setValue("MainWindow/clickConnectMode", _actions->clickConnectModeAction->isChecked());
    _settings->setValue("MainWindow/showGraphConnectionPoints", _actions->showGraphConnectionPointsAction->isChecked());
    _settings->setValue("MainWindow/showGraphBoundingBoxes", _actions->showGraphBoundingBoxesAction->isChecked());

    //close any open properties panel editor window
    _propertiesPanel->launchEditor(nullptr);

    //cleanup widgets which may use plugins or the server
    //this includes active graph blocks and eval engines
    poco_information(_logger, "Shutdown graph editor");
    delete _editorTabs;

    //unload the plugins
    //increase the log level to avoid deinit verbose
    poco_information(_logger, "Unload Pothos plugins");
    Poco::Logger::get("").setLevel(Poco::Message::PRIO_INFORMATION);
    Pothos::deinit();

    //stop the server
    _server = Pothos::RemoteServer();
}

void MainWindow::handleInitDone(void)
{
    _splash->postMessage(tr("Completing initialization..."));
    _splash->finish(this);
    poco_information(_logger, "Initialization complete");
}

void MainWindow::handleNewTitleSubtext(const QString &s)
{
    this->setWindowTitle("Pothos GUI - " + s);
}

void MainWindow::handleShowAbout(void)
{
    QMessageBox::about(this, "About Pothos", QString(
        "Pothos v%1\n"
        "Install %2\n"
        "www.pothosware.com")
        .arg(QString::fromStdString(Pothos::System::getApiVersion()))
        .arg(QString::fromStdString(Pothos::System::getRootPath())));
}

void MainWindow::handleShowAboutQt(void)
{
    QMessageBox::aboutQt(this);
}

void MainWindow::handleColorsDialogAction(void)
{
    auto dialog = new ColorsDialog(this);
    dialog->exec();
    delete dialog;
}

void MainWindow::handleFullScreenViewAction(const bool toggle)
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

void MainWindow::handleReloadPlugins(void)
{
    //close any open properties panel editor window
    _propertiesPanel->launchEditor(nullptr);

    //stop evaluation on all graph editor
    for (int i = 0; i < _editorTabs->count(); i++)
    {
        auto editor = dynamic_cast<GraphEditor *>(_editorTabs->widget(i));
        if (editor != nullptr) editor->stopEvaluation();
    }

    //clear the block cache
    _blockCache->clear();

    //restart the local server
    this->setupServer();

    //reload the block cache
    _blockCache->update();

    //start evaluation on all graph editor
    for (int i = 0; i < _editorTabs->count(); i++)
    {
        auto editor = dynamic_cast<GraphEditor *>(_editorTabs->widget(i));
        if (editor != nullptr) editor->restartEvaluation();
    }

    poco_information(Poco::Logger::get("PothosGui.MainWindow"), "Reload plugins complete");
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    emit this->exitBegin(event);
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
}

void MainWindow::setupServer(void)
{
    //spawn a new server if we previously spawned one
    bool spawnNewServer = bool(_server);

    //shutdown a previously opened server
    _server = Pothos::RemoteServer();

    //test connect to an existing server on localhost
    if (not spawnNewServer) try
    {
        Pothos::RemoteClient client("tcp://"+Pothos::Util::getLoopbackAddr());
    }
    catch (const Pothos::RemoteClientError &)
    {
        spawnNewServer = true;
    }

    //make a server and node that is temporary with this process
    //TODO make server background so it does not close with process
    _server = Pothos::RemoteServer("tcp://"+Pothos::Util::getLoopbackAddr(Pothos::RemoteServer::getLocatorPort()));

    //perform a test connection to the server
    Pothos::RemoteClient client("tcp://"+Pothos::Util::getLoopbackAddr()); //now it should connect to the new server
}
