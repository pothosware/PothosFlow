// Copyright (c) 2014-2021 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <QString>
#include <QMenu>
#include <QPointer>

class AffinityZonesDock;

/*!
 * A menu with options that reflect the active affinity zones.
 */
class AffinityZonesMenu : public QMenu
{
    Q_OBJECT
public:
    AffinityZonesMenu(AffinityZonesDock *affinityPanel, QWidget *parent);

signals:
    //! emitted when a submenu of that zone name is clicked
    void zoneClicked(const QString &);

private slots:
    void handleZonesChanged(void);

private:
    QPointer<AffinityZonesDock> _dock;
};
