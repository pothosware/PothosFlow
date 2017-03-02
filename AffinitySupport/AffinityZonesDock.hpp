// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <QDockWidget>
#include <QStringList>
#include <QJsonObject>
#include <QColor>

class QMenu;
class QComboBox;
class QLineEdit;
class QPushButton;
class QTabWidget;
class QSignalMapper;
class AffinityZoneEditor;
class HostExplorerDock;

//! Top level dock widget for affinity designer
class AffinityZonesDock : public QDockWidget
{
    Q_OBJECT
public:

    //! Get the global affinity zone dock
    static AffinityZonesDock *global(void);

    AffinityZonesDock(QWidget *parent, HostExplorerDock *hostExplorer);

    //! Make a new affinity selection menu that updates with this panel's configuration
    QMenu *makeMenu(QWidget *parent);

    //! Make a new affinity combo box that updates with this panel's configuration
    QComboBox *makeComboBox(QWidget *parent);

    //! query the list of available zones
    QStringList zones(void) const;

    //! Get the color for a particular zone
    QColor zoneToColor(const QString &zone);

    //! Get the config for a particular zone
    QJsonObject zoneToConfig(const QString &zone);

signals:

    //! emitted when zones are created, destroyed, changed
    void zonesChanged(void);

    //! emitted when a particular zone is modified
    void zoneChanged(const QString &zone);

private slots:
    void handleCreateZone(void);
    void handleTabCloseRequested(int);
    void handleZoneEditorChanged(void)
    {
        this->saveAffinityZoneEditorsState();
    }
    void handleTabSelectionChanged(int);
    void updateTabColors(void);

private:
    AffinityZoneEditor *createZoneFromName(const QString &name);

    void ensureDefault(void);

    void initAffinityZoneEditors(void);

    void saveAffinityZoneEditorsState(void);

    void handleErrorMessage(const QString &errMsg);

    HostExplorerDock *_hostExplorerDock;
    QSignalMapper *_mapper;
    QLineEdit *_zoneEntry;
    QPushButton *_createButton;
    QTabWidget *_editorsTabs;
};
