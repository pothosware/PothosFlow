// Copyright (c) 2013-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "PothosGuiUtils.hpp" //makeIconFromTheme
#include "MainWindow/MainSplash.hpp"
#include <QApplication>

static PothosGuiMainSplash *globalMainSplash = nullptr;

PothosGuiMainSplash *PothosGuiMainSplash::global(void)
{
    return globalMainSplash;
}

PothosGuiMainSplash::PothosGuiMainSplash(QWidget *parent):
    QSplashScreen(parent, QPixmap(makeIconPath("PothosSplash.png")))
{
    globalMainSplash = this;
}

void PothosGuiMainSplash::postMessage(const QString &msg)
{
    this->showMessage(msg, Qt::AlignLeft | Qt::AlignBottom);
    QApplication::instance()->processEvents();
}
