// Copyright (c) 2013-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "PothosGuiUtils.hpp" //makeIconFromTheme
#include "MainWindow/MainMenu.hpp"
#include "MainWindow/MainActions.hpp"
#include "AffinitySupport/AffinityZonesDock.hpp"
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>

static PothosGuiMainMenu *globalMainMenu = nullptr;

PothosGuiMainMenu *PothosGuiMainMenu::global(void)
{
    return globalMainMenu;
}

PothosGuiMainMenu::PothosGuiMainMenu(QMainWindow *parent, PothosGuiMainActions *actions):
    QObject(parent)
{
    globalMainMenu = this;

    fileMenu = parent->menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(actions->newAction);
    fileMenu->addAction(actions->openAction);
    fileMenu->addAction(actions->saveAction);
    fileMenu->addAction(actions->saveAsAction);
    fileMenu->addAction(actions->saveAllAction);
    fileMenu->addAction(actions->reloadAction);
    fileMenu->addAction(actions->closeAction);
    fileMenu->addSeparator();
    fileMenu->addAction(actions->exitAction);

    editMenu = parent->menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(actions->undoAction);
    editMenu->addAction(actions->redoAction);
    editMenu->addSeparator();
    editMenu->addAction(actions->cutAction);
    editMenu->addAction(actions->copyAction);
    editMenu->addAction(actions->pasteAction);
    editMenu->addAction(actions->deleteAction);
    editMenu->addAction(actions->selectAllAction);
    editMenu->addSeparator();
    editMenu->addAction(actions->findAction);
    editMenu->addSeparator();
    editMenu->addAction(actions->enableAction);
    editMenu->addAction(actions->disableAction);
    editMenu->addAction(actions->reevalAction);
    editMenu->addSeparator();
    editMenu->addAction(actions->rotateLeftAction);
    editMenu->addAction(actions->rotateRightAction);
    editMenu->addSeparator();
    editMenu->addAction(actions->incrementAction);
    editMenu->addAction(actions->decrementAction);
    editMenu->addSeparator();
    editMenu->addAction(actions->objectPropertiesAction);
    editMenu->addAction(actions->graphPropertiesAction);
    editMenu->addSeparator();
    auto pageMenu = editMenu->addMenu(tr("Graph page options..."));
    pageMenu->addAction(actions->createGraphPageAction);
    pageMenu->addAction(actions->renameGraphPageAction);
    pageMenu->addAction(actions->deleteGraphPageAction);
    pageMenu->addSeparator();
    pageMenu->addAction(actions->inputBreakerAction);
    pageMenu->addAction(actions->outputBreakerAction);
    moveGraphObjectsMenu = editMenu->addMenu(makeIconFromTheme("transform-move"), tr("Move graph objects..."));
    affinityZoneMenu = AffinityZonesDock::global()->makeMenu(editMenu);
    editMenu->addMenu(affinityZoneMenu);
    insertGraphWidgetsMenu = editMenu->addMenu(makeIconFromTheme("insert-image"), tr("Insert graph widgets..."));

    executeMenu = parent->menuBar()->addMenu(tr("&Execute"));
    executeMenu->addSeparator();
    executeMenu->addAction(actions->activateTopologyAction);
    executeMenu->addAction(actions->showRenderedGraphAction);
    executeMenu->addAction(actions->showTopologyStatsAction);

    viewMenu = parent->menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(actions->zoomInAction);
    viewMenu->addAction(actions->zoomOutAction);
    viewMenu->addAction(actions->zoomOriginalAction);
    viewMenu->addSeparator();
    viewMenu->addAction(actions->fullScreenViewAction);
    viewMenu->addSeparator();

    toolsMenu = parent->menuBar()->addMenu(tr("&Tools"));

    configMenu = toolsMenu->addMenu(tr("&Config"));
    configMenu->addAction(actions->showPortNamesAction);
    configMenu->addAction(actions->eventPortsInlineAction);
    configMenu->addAction(actions->clickConnectModeAction);

    debugMenu = toolsMenu->addMenu(tr("&Debug"));
    debugMenu->addAction(actions->showGraphConnectionPointsAction);
    debugMenu->addAction(actions->showGraphBoundingBoxesAction);

    helpMenu = parent->menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(actions->showAboutAction);
    helpMenu->addAction(actions->showAboutQtAction);
    helpMenu->addAction(actions->showColorsDialogAction);
}
