// Copyright (c) 2013-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <Pothos/Remote.hpp>
#include <QMainWindow>
#include <QString>
#include <QMap>
#include <Poco/Logger.h>

class QCloseEvent;
class QShowEvent;
class MainSplash;
class MainSettings;
class MainActions;
class BlockCache;
class GraphEditorTabs;
class PropertiesPanelDock;

/*!
 * The MainWindow is the entry point for the entire GUI application.
 * It sets up the various top level dock widgets, editor tabs,
 * creates main actions and menu items, and loads Pothos plugins.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:

    //! Global access to main window
    static MainWindow *global(void);

    MainWindow(QWidget *parent);

    ~MainWindow(void);

    void setWindowTitle(const QString &s);

signals:
    void initDone(void);
    void exitBegin(QCloseEvent *);

private slots:
    void handleInitDone(void);
    void handleShowAbout(void);
    void handleShowAboutQt(void);
    void handleColorsDialogAction(void);
    void handleFullScreenViewAction(const bool);
    void handleReloadPlugins(void);

protected:
    void closeEvent(QCloseEvent *event);

    void showEvent(QShowEvent *event);

private:
    Poco::Logger &_logger;
    MainSplash *_splash;
    MainSettings *_settings;
    MainActions *_actions;
    Pothos::RemoteServer _server;
    BlockCache *_blockCache;
    GraphEditorTabs *_editorTabs;
    PropertiesPanelDock *_propertiesPanel;

    //restoring from full screen
    std::map<QWidget *, bool> _widgetToOldVisibility;

    //! Setup server for scratch process
    void setupServer(void);
};
