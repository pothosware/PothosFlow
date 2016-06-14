// Copyright (c) 2013-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <QObject>

class QAction;

/*!
 * This class contains top level QActions used in the menu
 * and connected to slots throughout the application.
 */
class MainActions : public QObject
{
    Q_OBJECT
public:

    //! Global access to main actions
    static MainActions *global(void);

    MainActions(QObject *parent);

    QAction *newAction;
    QAction *openAction;
    QAction *saveAction;
    QAction *saveAsAction;
    QAction *saveAllAction;
    QAction *reloadAction;
    QAction *closeAction;
    QAction *exitAction;
    QAction *undoAction;
    QAction *redoAction;
    QAction *enableAction;
    QAction *disableAction;
    QAction *reevalAction;
    QAction *cutAction;
    QAction *copyAction;
    QAction *pasteAction;
    QAction *deleteAction;
    QAction *selectAllAction;
    QAction *objectPropertiesAction;
    QAction *graphPropertiesAction;
    QAction *createGraphPageAction;
    QAction *renameGraphPageAction;
    QAction *deleteGraphPageAction;
    QAction *inputBreakerAction;
    QAction *outputBreakerAction;
    QAction *rotateLeftAction;
    QAction *rotateRightAction;
    QAction *zoomInAction;
    QAction *zoomOutAction;
    QAction *zoomOriginalAction;
    QAction *showAboutAction;
    QAction *showAboutQtAction;
    QAction *findAction;
    QAction *showGraphConnectionPointsAction;
    QAction *showGraphBoundingBoxesAction;
    QAction *showRenderedGraphAction;
    QAction *showTopologyStatsAction;
    QAction *activateTopologyAction;
    QAction *showPortNamesAction;
    QAction *eventPortsInlineAction;
    QAction *clickConnectModeAction;
    QAction *showColorsDialogAction;
    QAction *incrementAction;
    QAction *decrementAction;
    QAction *fullScreenViewAction;
    QAction *reloadPluginsAction;
};
