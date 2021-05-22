// Copyright (c) 2018-2019 Josh Blum
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
#include <QApplication>
#include "MainWindow/MainActions.hpp"
#include "MainWindow/MainWindow.hpp"
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

    QDialog *dialog(void) const
    {
        return _dialog;
    }

    const QString &label(void) const
    {
        return _label;
    }

    void setLabel(const QString &label)
    {
        _label = label;
        this->internalUpdate();
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
            _dialog->deleteLater(); //delete later, may have events pending
            _dialog.clear(); //clear the qpointer so it has a null state
        }
        else //undock into new dialog
        {
            _dialog = new QDialog(this);
            _dialog->installEventFilter(_tabs);

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
        this->internalUpdate();
    }

    void internalUpdate(void)
    {
        //sync the tab window's title text to the dialog
        if (_dialog)
        {
            _dialog->setWindowTitle(QString("Pothos Flow - [%1] %2").arg(_label).arg(_tabs->windowTitle()));
            _dialog->setWindowModified(_tabs->isWindowModified());
        }

        //update the tab bar for this widget
        const auto index = this->tabIndex();
        if (index == -1) return; //shouldn't happen

        //set the tool tip
        auto tabBar = _tabs->tabBar();
        const bool docked = this->isDocked();
        tabBar->setTabToolTip(index, (docked?tr("Undock tab: %1"):tr("Restore tab: %1")).arg(_label));

        //set the button style sheet
        auto button = qobject_cast<QPushButton *>(tabBar->tabButton(index, QTabBar::RightSide));
        if (button == nullptr) return; //didn't allocate button yet
        if (docked != button->isChecked()) button->setChecked(docked); //sync state
    }

    int tabIndex(void) const
    {
        for (int index = 0; index < _tabs->count(); index++)
        {
            if (_tabs->widget(index) == this) return index;
        }
        return -1;
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
    MainWindow::global()->installEventFilter(this);
    this->connect(_mapper, SIGNAL(mapped(QWidget *)), this, SLOT(handleUndockButton(QWidget *)));
}

DockingTabWidget::~DockingTabWidget(void)
{
    return;
}

bool DockingTabWidget::isActive(void) const
{
    //active if one of the dialogs is active
    auto activeWindow = QApplication::activeWindow();
    if (activeWindow != nullptr)
    {
        for (int index = 0; index < this->count(); index++)
        {
            if (this->page(index)->dialog() == activeWindow) return true;
        }
    }

    //use default visibility for this widget when the main window is active
    if (activeWindow == MainWindow::global()) return QTabWidget::isVisible();

    //otherwise is not active
    return false;
}

void DockingTabWidget::clear(void)
{
    for (int index = 0; index < this->count(); index++)
    {
        //de-parent the actual widget and delete the page
        //its the job of the caller to delete with widget
        auto page = this->page(index);
        page->widget()->setParent(nullptr);
        page->deleteLater();
    }
    return QTabWidget::clear();
}

int DockingTabWidget::activeIndex(void) const
{
    auto activeWindow = QApplication::activeWindow();
    if (activeWindow != nullptr)
    {
        for (int index = 0; index < this->count(); index++)
        {
            if (this->page(index)->dialog() == activeWindow) return index;
        }
    }
    return this->currentIndex();
}

void DockingTabWidget::setWindowModified(const bool modified)
{
    QTabWidget::setWindowModified(modified);
    this->internalUpdate(); //updates all dialogs
}

void DockingTabWidget::setWindowTitle(const QString &title)
{
    QTabWidget::setWindowTitle(title);
    this->internalUpdate(); //updates all dialogs
}

QWidget *DockingTabWidget::currentWidget(void) const
{
    auto container = qobject_cast<DockingPage *>(QTabWidget::currentWidget());
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

QVariant DockingTabWidget::saveWidgetState(void) const
{
    QVariantMap data;
    data["index"] = this->currentIndex();
    QVariantList tabs;
    for (int index = 0; index < this->count(); index++)
    {
        QVariantMap tabData;
        tabData["docked"] = this->page(index)->isDocked();
        tabData["geometry"] = this->saveGeometry(index);
        tabs.push_back(tabData);
    }
    data["tabs"] = tabs;
    return QVariant(data);
}

void DockingTabWidget::restoreWidgetState(const QVariant &state)
{
    if (state.isNull()) return;
    const auto data = state.toMap();
    if (data.isEmpty()) return;
    this->setCurrentIndex(data["index"].toInt());
    const auto tabs = data["tabs"].toList();
    for (int index = 0; index < std::min(static_cast<int>(tabs.size()), this->count()); index++)
    {
        const auto tabData = tabs[index].toMap();
        this->page(index)->setDocked(tabData["docked"].toBool());
        this->restoreGeometry(index, tabData["geometry"].toByteArray());
    }
}

void DockingTabWidget::handleUndockButton(QWidget *widget)
{
    auto container = qobject_cast<DockingPage *>(widget);
    container->setDocked(not container->isDocked());

    //when docking: select the tab that just got docked
    //when undocking: select a different docked tab, since this tab now appears blank
    for (int index = 0; index < this->count(); index++)
    {
        if ((container->isDocked() and this->page(index) == container) or
            (not container->isDocked() and this->page(index)->isDocked()))
        {
            this->setCurrentIndex(index);
            break;
        }
    }
}

void DockingTabWidget::tabInserted(int index)
{
    //create custom button for dock/undock
    auto button = new QPushButton(this->tabBar());
    button->resize(16, 16);
    button->setCheckable(true);
    button->setChecked(true);
    static const auto buttonStyle =
        QString("QPushButton{border-image: url(%1);}").arg(makeIconPath("dockingtab-dock.png"))+
        QString("QPushButton:hover{border-image: url(%1);}").arg(makeIconPath("dockingtab-dock-hover.png"))+
        QString("QPushButton:pressed{border-image: url(%1);}").arg(makeIconPath("dockingtab-dock-down.png"))+
        QString("QPushButton:checked{border-image: url(%1);}").arg(makeIconPath("dockingtab-undock.png"))+
        QString("QPushButton:checked:hover{border-image: url(%1);}").arg(makeIconPath("dockingtab-undock-hover.png"))+
        QString("QPushButton:checked:pressed{border-image: url(%1);}").arg(makeIconPath("dockingtab-undock-down.png"));
    button->setStyleSheet(buttonStyle);
    _mapper->setMapping(button, QTabWidget::widget(index));
    connect(button, SIGNAL(clicked()), _mapper, SLOT(map()));
    this->tabBar()->setTabButton(index, QTabBar::RightSide, button);
    this->page(index)->internalUpdate();

    QTabWidget::tabInserted(index);
}

void DockingTabWidget::tabRemoved(int index)
{
    QTabWidget::tabRemoved(index);
}

bool DockingTabWidget::eventFilter(QObject *, QEvent *event)
{
    //emit signal if any windows became active (dialogs or main window)
    if (event->type() == QEvent::WindowActivate) this->activeChanged();
    return false;
}

DockingTabWidget::DockingPage *DockingTabWidget::page(const int index) const
{
    return qobject_cast<DockingPage *>(QTabWidget::widget(index));
}

void DockingTabWidget::internalUpdate(void)
{
    for (int index = 0; index < this->count(); index++)
    {
        this->page(index)->internalUpdate();
    }
}

#include "DockingTabWidget.moc"
