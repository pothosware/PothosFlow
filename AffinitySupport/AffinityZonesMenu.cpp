// Copyright (c) 2014-2021 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "AffinitySupport/AffinityZonesMenu.hpp"
#include "AffinitySupport/AffinityZonesDock.hpp"

AffinityZonesMenu::AffinityZonesMenu(AffinityZonesDock *dock, QWidget *parent):
    QMenu(tr("Graph blocks affinity..."), parent),
    _dock(dock)
{
    connect(_dock, &AffinityZonesDock::zonesChanged, this, &AffinityZonesMenu::handleZonesChanged);
    this->handleZonesChanged(); //init
}

void AffinityZonesMenu::handleZonesChanged(void)
{
    this->clear();

    //clear affinity setting
    auto clearAction = this->addAction(tr("Clear affinity"));
    connect(clearAction, &QAction::triggered, [=](int){emit this->zoneClicked("");});

    //gui affinity setting
    auto guiAction = this->addAction(tr("GUI affinity"));
    connect(guiAction, &QAction::triggered, [=](int){emit this->zoneClicked("gui");});

    if (_dock) for (const auto &name : _dock->zones())
    {
        auto action = this->addAction(tr("Apply %1").arg(name));
        connect(action, &QAction::triggered, [=](int){emit this->zoneClicked(name);});
    }
}
