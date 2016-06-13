// Copyright (c) 2013-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <QMainWindow>
#include <QString>
#include <QMap>

class QCloseEvent;
class QShowEvent;
class PothosGuiMainSettings;
class PothosGuiMainActions;

class PothosGuiMainWindow : public QMainWindow
{
    Q_OBJECT
public:

    PothosGuiMainWindow(QWidget *parent);

    ~PothosGuiMainWindow(void);

signals:
    void initDone(void);
    void exitBegin(QCloseEvent *);

private slots:

    void handleNewTitleSubtext(const QString &s);
    void handleShowAbout(void);
    void handleShowAboutQt(void);
    void handleColorsDialogAction(void);
    void handleFullScreenViewAction(const bool);

protected:
    void closeEvent(QCloseEvent *event);

    void showEvent(QShowEvent *event);

private:
    PothosGuiMainSettings *_settings;
    PothosGuiMainActions *_actions;

    //restoring from full screen
    std::map<QWidget *, bool> _widgetToOldVisibility;
};
