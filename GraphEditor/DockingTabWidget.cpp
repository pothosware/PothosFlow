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
#include <QAction>
#include <QShortcut>
#include "MainWindow/MainActions.hpp"
#include <iostream>

/***********************************************************************
 * The docking page holds the actual internal tab page widget
 * but can display it inside of a dialog when in undocked mode.
 **********************************************************************/
class DockingTabWidget::DockingPage : public QWidget
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
            _dialogGeometry = _dialog->saveGeometry();
            delete _dialog;
        }
        else //undock into new dialog
        {
            _dialog = new QDialog(this);

            //the dialog will not have top level main actions
            //and will not react to key shortcuts used by the main window
            //to deal with this, iterate the main actions
            //and tie in any actions with key shortcuts
            for (auto child : MainActions::global()->children())
            {
                auto action = qobject_cast<QAction *>(child);
                if (action == nullptr) continue;
                auto keys = action->shortcut();
                if (keys.isEmpty()) continue;
                auto shortcut = new QShortcut(_dialog);
                shortcut->setKey(keys);
                this->connect(shortcut, &QShortcut::activated, action, &QAction::trigger);
            }

            auto layout = new QVBoxLayout(_dialog);
            _widget->setParent(_dialog);
            layout->addWidget(_widget);
            if (_dialogGeometry.isEmpty()) _dialog->resize(_tabs->size());
            else _dialog->restoreGeometry(_dialogGeometry);
            this->connect(_dialog, &QDialog::finished, this, &DockingPage::handleDialogFinished);
            _dialog->show();
            _widget->show();
        }
    }

    void internalUpdate(void)
    {
        if (_dialog)
        {
            _dialog->setWindowTitle(QString("Pothos Flow - [%1] %2").arg(_label).arg(this->windowTitle()));
            _dialog->setWindowModified(this->isWindowModified());
        }
    }

    QByteArray saveGeometry(void) const
    {
        if (this->isDocked()) return _dialogGeometry;
        return _dialog->saveGeometry();
    }

    bool restoreGeometry(const QByteArray &geometry)
    {
        if (this->isDocked())
        {
            _dialogGeometry = geometry;
            return true;
        }
        return _dialog->restoreGeometry(geometry);
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
    QByteArray _dialogGeometry;
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

void DockingTabWidget::setWindowModified(const bool modified)
{
    QTabWidget::setWindowModified(modified);
    this->internalUpdate();
}

void DockingTabWidget::setWindowTitle(const QString &title)
{
    QTabWidget::setWindowTitle(title);
    this->internalUpdate();
}

QWidget *DockingTabWidget::currentWidget(void) const
{
    auto container = reinterpret_cast<DockingPage *>(QTabWidget::currentWidget());
    return container->widget();
}

QWidget *DockingTabWidget::widget(int index) const
{
    return this->page(index)->widget();
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
    this->page(index)->setLabel(label);
    this->internalUpdate();
    return QTabWidget::setTabText(index, label);
}

QString DockingTabWidget::tabText(int index) const
{
    return this->page(index)->label();
}

bool DockingTabWidget::isDocked(int index) const
{
    return this->page(index)->isDocked();
}

void DockingTabWidget::setDocked(int index, const bool docked)
{
    return this->page(index)->setDocked(docked);
}

QByteArray DockingTabWidget::saveGeometry(int index) const
{
    return this->page(index)->saveGeometry();
}

bool DockingTabWidget::restoreGeometry(int index, const QByteArray &geometry)
{
    return this->page(index)->restoreGeometry(geometry);
}

void DockingTabWidget::handleUndockButton(QWidget *widget)
{
    auto container = reinterpret_cast<DockingPage *>(widget);
    container->setDocked(not container->isDocked());

    //select a different docked tab, since this tab now appears blank
    if (container->isDocked()) return; //its docked, skip selection change
    for (int index = 0; index < this->count(); index++)
    {
        if (not this->page(index)->isDocked()) continue;
        this->setCurrentIndex(index);
        break;
    }

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

DockingTabWidget::DockingPage *DockingTabWidget::page(const int index) const
{
    return reinterpret_cast<DockingPage *>(QTabWidget::widget(index));
}

void DockingTabWidget::internalUpdate(void)
{
    for (int index = 0; index < this->count(); index++)
    {
        auto container = this->page(index);
        container->setWindowTitle(this->windowTitle());
        container->setWindowModified(this->isWindowModified());
        container->internalUpdate();
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
