// Copyright (c) 2013-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "MainWindow/MainToolBar.hpp"
#include "MainWindow/MainActions.hpp"

MainToolBar::MainToolBar(QWidget *parent, MainActions *actions):
    QToolBar(tr("Main Tool Bar"), parent)
{
    this->setObjectName("MainToolBar");

    this->addAction(actions->newAction);
    this->addAction(actions->openAction);
    this->addAction(actions->saveAction);
    this->addAction(actions->saveAsAction);
    this->addAction(actions->saveAllAction);
    this->addAction(actions->reloadAction);
    this->addAction(actions->closeAction);
    this->addSeparator();

    this->addAction(actions->zoomInAction);
    this->addAction(actions->zoomOutAction);
    this->addAction(actions->zoomOriginalAction);
    this->addAction(actions->fullScreenViewAction);
    this->addSeparator();

    this->addAction(actions->undoAction);
    this->addAction(actions->redoAction);
    this->addSeparator();

    this->addAction(actions->activateTopologyAction);
    this->addAction(actions->reloadPluginsAction);
    this->addSeparator();

    this->addAction(actions->cutAction);
    this->addAction(actions->copyAction);
    this->addAction(actions->pasteAction);
    this->addAction(actions->deleteAction);
    this->addSeparator();

    this->addAction(actions->selectAllAction);
    this->addSeparator();

    this->addAction(actions->findAction);
    this->addSeparator();

    this->addAction(actions->rotateLeftAction);
    this->addAction(actions->rotateRightAction);
    this->addSeparator();

    this->addAction(actions->objectPropertiesAction);
}
