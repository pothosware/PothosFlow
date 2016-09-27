// Copyright (c) 2013-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "MainWindow/MainActions.hpp"
#include "MainWindow/IconUtils.hpp"
#include <QAction>

static MainActions *globalMainActions = nullptr;

MainActions *MainActions::global(void)
{
    return globalMainActions;
}

MainActions::MainActions(QObject *parent):
    QObject(parent)
{
    globalMainActions = this;

    newAction = new QAction(makeIconFromTheme("document-new"), tr("&New"), this);
    newAction->setShortcut(QKeySequence::New);

    openAction = new QAction(makeIconFromTheme("document-open"), tr("&Open"), this);
    openAction->setShortcut(QKeySequence::Open);

    saveAction = new QAction(makeIconFromTheme("document-save"), tr("&Save"), this);
    saveAction->setShortcut(QKeySequence::Save);

    saveAsAction = new QAction(makeIconFromTheme("document-save-as"), tr("Save &As"), this);
    saveAsAction->setShortcut(QKeySequence::SaveAs);

    saveAllAction = new QAction(makeIconFromTheme("document-save-all"), tr("Save A&ll"), this);
    saveAllAction->setShortcut(QKeySequence("CTRL+SHIFT+A"));

    reloadAction = new QAction(makeIconFromTheme("document-revert"), tr("&Reload"), this);
    QList<QKeySequence> reloadShortcuts;
    reloadShortcuts.push_back(QKeySequence::Refresh);
    reloadShortcuts.push_back(QKeySequence("CTRL+R"));
    if (reloadShortcuts.front().matches(reloadShortcuts.back()) == QKeySequence::ExactMatch) reloadShortcuts.pop_back();
    reloadAction->setShortcuts(reloadShortcuts);

    closeAction = new QAction(makeIconFromTheme("document-close"), tr("&Close"), this);
    closeAction->setShortcut(QKeySequence::Close);

    exitAction = new QAction(makeIconFromTheme("application-exit"), tr("&Exit Pothos GUI"), this);
    exitAction->setShortcut(QKeySequence::Quit);

    undoAction = new QAction(makeIconFromTheme("edit-undo"), tr("&Undo"), this);
    undoAction->setShortcut(QKeySequence::Undo);

    redoAction = new QAction(makeIconFromTheme("edit-redo"), tr("&Redo"), this);
    QList<QKeySequence> redoShortcuts;
    redoShortcuts.push_back(QKeySequence::Redo);
    redoShortcuts.push_back(QKeySequence("CTRL+Y"));
    if (redoShortcuts.front().matches(redoShortcuts.back()) == QKeySequence::ExactMatch) redoShortcuts.pop_back();
    redoAction->setShortcuts(redoShortcuts);

    enableAction = new QAction(makeIconFromTheme("document-import"), tr("Enable"), this);
    enableAction->setStatusTip(tr("Enable selected graph objects"));
    enableAction->setShortcut(QKeySequence(Qt::Key_E));

    disableAction = new QAction(makeIconFromTheme("document-export"), tr("Disable"), this);
    disableAction->setStatusTip(tr("Disable selected graph objects"));
    disableAction->setShortcut(QKeySequence(Qt::Key_D));

    reevalAction = new QAction(makeIconFromTheme("edit-clear-history"), tr("Re-eval"), this);
    reevalAction->setStatusTip(tr("Re-evaluate selected graph objects"));
    reevalAction->setShortcut(QKeySequence(Qt::Key_R));

    cutAction = new QAction(makeIconFromTheme("edit-cut"), tr("Cu&t"), this);
    cutAction->setShortcut(QKeySequence::Cut);

    copyAction = new QAction(makeIconFromTheme("edit-copy"), tr("&Copy"), this);
    copyAction->setShortcut(QKeySequence::Copy);

    pasteAction = new QAction(makeIconFromTheme("edit-paste"), tr("&Paste"), this);
    pasteAction->setShortcut(QKeySequence::Paste);

    deleteAction = new QAction(makeIconFromTheme("edit-delete"), tr("&Delete"), this);
    deleteAction->setShortcut(QKeySequence::Delete);

    selectAllAction = new QAction(makeIconFromTheme("edit-select-all"), tr("Select &All"), this);
    selectAllAction->setShortcut(QKeySequence::SelectAll);

    objectPropertiesAction = new QAction(makeIconFromTheme("document-properties"), tr("&Object Properties"), this);
    objectPropertiesAction->setShortcut(Qt::Key_Return);
    objectPropertiesAction->setShortcutContext(Qt::WidgetShortcut); //disable shortcut, see GraphDraw::keyPressEvent()

    graphPropertiesAction = new QAction(makeIconFromTheme("document-properties"), tr("&Graph Properties"), this);

    createGraphPageAction = new QAction(makeIconFromTheme("document-new"), tr("Create new graph page"), this);

    renameGraphPageAction = new QAction(makeIconFromTheme("edit-rename"), tr("Rename this graph page"), this);

    deleteGraphPageAction = new QAction(makeIconFromTheme("edit-delete"), tr("Delete this graph page"), this);

    inputBreakerAction = new QAction(makeIconFromTheme("edit-table-insert-column-right"), tr("Insert input breaker"), this);

    outputBreakerAction = new QAction(makeIconFromTheme("edit-table-insert-column-left"), tr("Insert output breaker"), this);

    rotateLeftAction = new QAction(makeIconFromTheme("object-rotate-left"), tr("Rotate Left"), this);
    rotateLeftAction->setShortcut(Qt::Key_Left);
    rotateLeftAction->setShortcutContext(Qt::WidgetShortcut); //disable shortcut, see GraphDraw::keyPressEvent()

    rotateRightAction = new QAction(makeIconFromTheme("object-rotate-right"), tr("Rotate Right"), this);
    rotateRightAction->setShortcut(Qt::Key_Right);
    rotateRightAction->setShortcutContext(Qt::WidgetShortcut); //disable shortcut, see GraphDraw::keyPressEvent()

    zoomInAction = new QAction(makeIconFromTheme("zoom-in"), tr("Zoom in"), this);
    zoomInAction->setShortcut(QKeySequence::ZoomIn);

    zoomOutAction = new QAction(makeIconFromTheme("zoom-out"), tr("Zoom out"), this);
    zoomOutAction->setShortcut(QKeySequence::ZoomOut);

    zoomOriginalAction = new QAction(makeIconFromTheme("zoom-original"), tr("Normal size"), this);
    zoomOriginalAction->setShortcut(QKeySequence("CTRL+0"));

    findAction = new QAction(makeIconFromTheme("edit-find"), tr("&Find"), this);
    findAction->setShortcut(QKeySequence::Find);

    showGraphConnectionPointsAction = new QAction(tr("Show graph &connection points"), this);
    showGraphConnectionPointsAction->setCheckable(true);

    showGraphBoundingBoxesAction = new QAction(tr("Show graph &bounding boxes"), this);
    showGraphBoundingBoxesAction->setCheckable(true);

    showRenderedGraphAction = new QAction(tr("Show rendered graph view"), this);

    showTopologyStatsAction = new QAction(tr("Show topology stats dump"), this);

    activateTopologyAction = new QAction(makeIconFromTheme("run-build"), tr("&Activate topology"), this);
    activateTopologyAction->setCheckable(true);
    activateTopologyAction->setShortcut(QKeySequence("F6"));

    showPortNamesAction = new QAction(tr("Show block port names"), this);
    showPortNamesAction->setCheckable(true);
    showPortNamesAction->setStatusTip(tr("Show the names of block IO ports on the graph"));

    eventPortsInlineAction = new QAction(tr("Inline block event ports"), this);
    eventPortsInlineAction->setCheckable(true);
    eventPortsInlineAction->setStatusTip(tr("Show block event ports inline with IO ports"));

    clickConnectModeAction = new QAction(tr("Click-connect create mode"), this);
    clickConnectModeAction->setCheckable(true);
    clickConnectModeAction->setStatusTip(tr("Connect ports using subsequent mouse clicks"));

    showAboutAction = new QAction(makeIconFromTheme("help-about"), tr("&About Pothos"), this);
    showAboutAction->setStatusTip(tr("Information about this version of Pothos"));

    showAboutQtAction = new QAction(makeIconFromTheme("help-about"), tr("About &Qt"), this);
    showAboutQtAction->setStatusTip(tr("Information about this version of QT"));

    showColorsDialogAction = new QAction(makeIconFromTheme("color-picker"), tr("&Colors Map"), this);
    showColorsDialogAction->setStatusTip(tr("Data type colors used for block properties and ports"));

    incrementAction = new QAction(makeIconFromTheme("list-add"), tr("Block &Increment"), this);
    incrementAction->setStatusTip(tr("Increment action on selected graph objects"));
    incrementAction->setShortcut(QKeySequence(Qt::Key_Plus));

    decrementAction = new QAction(makeIconFromTheme("list-remove"), tr("Block &Decrement"), this);
    decrementAction->setStatusTip(tr("Decrement action on selected graph objects"));
    decrementAction->setShortcut(QKeySequence(Qt::Key_Minus));

    fullScreenViewAction = new QAction(makeIconFromTheme("view-fullscreen"), tr("Full-screen view mode"), this);
    fullScreenViewAction->setCheckable(true);
    fullScreenViewAction->setStatusTip(tr("Maximize graph editor area, hide dock widgets"));
    fullScreenViewAction->setShortcut(QKeySequence(Qt::Key_F11));

    reloadPluginsAction = new QAction(makeIconFromTheme("view-refresh"), tr("Reload plugin registry"), this);
    reloadPluginsAction->setStatusTip(tr("Stop evaluation, reload plugins, resume evaluation"));
    reloadPluginsAction->setShortcut(QKeySequence("F8"));

    exportAction = new QAction(makeIconFromTheme("document-export"), tr("Export to JSON topology"), this);
    exportAction->setStatusTip(tr("Export the current design to the JSON topology format"));
    exportAction->setShortcut(QKeySequence("CTRL+E"));

    exportAsAction = new QAction(makeIconFromTheme("document-export"), tr("Export to JSON topology as..."), this);
    exportAsAction->setStatusTip(tr("Export the current design to the JSON topology format as..."));
    exportAsAction->setShortcut(QKeySequence("CTRL+SHIFT+E"));
}
