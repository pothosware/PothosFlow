// Copyright (c) 2013-2021 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "MainWindow/MainSplash.hpp"
#include "MainWindow/IconUtils.hpp"
#include <QApplication>

static MainSplash *globalMainSplash = nullptr;

MainSplash *MainSplash::global(void)
{
    return globalMainSplash;
}

MainSplash::MainSplash(QWidget *parent):
    QSplashScreen(
        #if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
        parent,
        #else
        parent->screen(), //old constructor deprecated 5.15
        #endif
        QPixmap(makeIconPath("PothosSplash.png")))
{
    globalMainSplash = this;
}

void MainSplash::postMessage(const QString &msg)
{
    this->showMessage(msg, Qt::AlignLeft | Qt::AlignBottom);
    QApplication::instance()->processEvents();
}
