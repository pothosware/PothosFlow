// Copyright (c) 2018-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "DockingTabWidget.hpp"
#include "MainWindow/IconUtils.hpp"
#include <QSignalMapper>
#include <QPushButton>
#include <QTabBar>
#include <QIcon>
#include <QEvent>
#include <QDialog>
#include <QVBoxLayout>
#include <QPointer>
#include <iostream>

/***********************************************************************
 * The docking page holds the actual internal tab page widget
 * but can display it inside of a dialog when in undocked mode.
 **********************************************************************/
class DockingPage : public QWidget
{
    Q_OBJECT
public:
    DockingPage(QWidget *widget, const QString &label, QTabWidget *parent):
        QWidget(parent),
        _layout(new QVBoxLayout(this)),
        _label(label),
        _widget(widget),
        _tabs(parent)
    {
        _layout->setSpacing(0);
        _layout->setContentsMargins(QMargins());
        _layout->addWidget(widget);
        _widget->setParent(this);
    }

    QWidget *widget(void) const
    {
        return _widget;
    }

    const QString &label(void) const
    {
        return _label;
    }

    void setLabel(const QString &label)
    {
        _label = label;
    }

    bool isDocked(void) const
    {
        return not _dialog;
    }

    void setDocked(const bool docked)
    {
        if (this->isDocked() == docked) return;
        if (docked) //new state is docked
        {
            _layout->addWidget(_widget);
            _widget->setParent(this);
            _dialogGeometry = _dialog->geometry();
            delete _dialog;
        }
        else //undock into new dialog
        {
            _dialog = new QDialog(this);
            auto layout = new QVBoxLayout(_dialog);
            _widget->setParent(_dialog);
            layout->addWidget(_widget);
            if (_dialogGeometry.isValid()) _dialog->setGeometry(_dialogGeometry);
            else _dialog->resize(_tabs->size());
            this->connect(_dialog, &QDialog::finished, this, &DockingPage::handleDialogFinished);
            _dialog->show();
            _widget->show();

            //TODO select a different tab if it exists
        }
    }

    void updateDialogTitle(const QString &title)
    {
        if (_dialog) _dialog->setWindowTitle(QString("[%1] %2").arg(_label).arg(title));
    }

private slots:
    void handleDialogFinished(int)
    {
        this->setDocked(true);
    }

private:
    QVBoxLayout *_layout;
    QPointer<QDialog> _dialog;
    QString _label;
    QWidget *_widget;
    QTabWidget *_tabs;
    QRect _dialogGeometry;
};

/***********************************************************************
 * Docking tab widget implementation
 **********************************************************************/
DockingTabWidget::DockingTabWidget(QWidget *parent):
    QTabWidget(parent),
    _mapper(new QSignalMapper(this))
{
    this->connect(_mapper, SIGNAL(mapped(QWidget *)), this, SLOT(handleUndockButton(QWidget *)));
}

DockingTabWidget::~DockingTabWidget(void)
{
    return;
}

void DockingTabWidget::setDialogTitle(const QString &title)
{
    _dialogTitle = title;
    this->internalUpdate();
}

bool DockingTabWidget::isDocked(const int index) const
{
    auto container = reinterpret_cast<DockingPage *>(QTabWidget::widget(index));
    return container->isDocked();
}

QWidget *DockingTabWidget::currentWidget(void) const
{
    auto container = reinterpret_cast<DockingPage *>(QTabWidget::currentWidget());
    return container->widget();
}

QWidget *DockingTabWidget::widget(int index) const
{
    auto container = reinterpret_cast<DockingPage *>(QTabWidget::widget(index));
    return container->widget();
}

int DockingTabWidget::addTab(QWidget *page, const QString &label)
{
    auto container = new DockingPage(page, label, this);
    return QTabWidget::addTab(container, label);
}

int DockingTabWidget::insertTab(int index, QWidget *page, const QString &label)
{
    auto container = new DockingPage(page, label, this);
    return QTabWidget::insertTab(index, container, label);
}

void DockingTabWidget::setTabText(int index, const QString &label)
{
    auto container = reinterpret_cast<DockingPage *>(QTabWidget::widget(index));
    container->setLabel(label);
    this->internalUpdate();
    return QTabWidget::setTabText(index, label);
}

QString DockingTabWidget::tabText(int index) const
{
    auto container = reinterpret_cast<DockingPage *>(QTabWidget::widget(index));
    return container->label();
}

void DockingTabWidget::handleUndockButton(QWidget *widget)
{
    auto container = reinterpret_cast<DockingPage *>(widget);
    container->setDocked(not container->isDocked());
    this->internalUpdate();
}

void DockingTabWidget::tabInserted(int index)
{
    //create custom button for dock/undock
    auto button = new QPushButton(this->tabBar());
    button->resize(16, 16);
    button->setCheckable(false);
    _mapper->setMapping(button, QTabWidget::widget(index));
    connect(button, SIGNAL(clicked()), _mapper, SLOT(map()));
    this->tabBar()->setTabButton(index, QTabBar::RightSide, button);

    this->internalUpdate();
    QTabWidget::tabInserted(index);
}

void DockingTabWidget::tabRemoved(int index)
{
    this->internalUpdate();
    QTabWidget::tabRemoved(index);
}

void DockingTabWidget::internalUpdate(void)
{
    for (int index = 0; index < this->count(); index++)
    {
        auto container = reinterpret_cast<DockingPage *>(QTabWidget::widget(index));
        container->updateDialogTitle(_dialogTitle);
        const bool docked = container->isDocked();

        auto button = reinterpret_cast<QPushButton *>(this->tabBar()->tabButton(index, QTabBar::RightSide));
        button->setToolTip((docked?tr("Undock tab: %1"):tr("Restore tab: %1")).arg(container->label()));
        const QString prefix = docked?"undock":"dock";
        button->setStyleSheet(
            QString("QPushButton{border-image: url(%1);}").arg(makeIconPath("dockingtab-"+prefix+".png"))+
            QString("QPushButton:hover{border-image: url(%1);}").arg(makeIconPath("dockingtab-"+prefix+"-hover.png"))+
            QString("QPushButton:pressed{border-image: url(%1);}").arg(makeIconPath("dockingtab-"+prefix+"-down.png")));
    }
}

#include "DockingTabWidget.moc"
