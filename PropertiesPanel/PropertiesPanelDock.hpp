// Copyright (c) 2014-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <QDockWidget>
#include <QPointer>

class QScrollArea;
class QPushButton;

class PropertiesPanelDock : public QDockWidget
{
    Q_OBJECT
public:

    //! Get access to the global properties panel dock
    static PropertiesPanelDock *global(void);

    PropertiesPanelDock(QWidget *parent);

signals:
    void replacePanel(void);

public slots:

    //! Load the editor based on the type of graph object
    void launchEditor(QObject *obj);

private slots:

    void handlePanelDestroyed(QObject *);

    void handleDeletePanel(void);

private:
    QPointer<QObject> _currentGraphObject;
    QPointer<QWidget> _propertiesPanel;
    QScrollArea *_scroll;
    QPushButton *_commitButton;
    QPushButton *_cancelButton;
};
