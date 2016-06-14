// Copyright (c) 2013-2016 Josh Blum
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
class GraphEditorTabs;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:

    MainWindow(QWidget *parent);

    ~MainWindow(void);

signals:
    void initDone(void);
    void exitBegin(QCloseEvent *);

private slots:
    void handleInitDone(void);
    void handleNewTitleSubtext(const QString &s);
    void handleShowAbout(void);
    void handleShowAboutQt(void);
    void handleColorsDialogAction(void);
    void handleFullScreenViewAction(const bool);

protected:
    void closeEvent(QCloseEvent *event);

    void showEvent(QShowEvent *event);

private:
    Poco::Logger &_logger;
    MainSplash *_splash;
    MainSettings *_settings;
    MainActions *_actions;
    Pothos::RemoteServer _server;
    GraphEditorTabs *_editorTabs;

    //restoring from full screen
    std::map<QWidget *, bool> _widgetToOldVisibility;
};
