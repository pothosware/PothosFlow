// Copyright (c) 2018-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <QTabWidget>

class QSignalMapper;

/*!
 * A customized tab widget with detachable tabs that can become independent windows.
 */
class DockingTabWidget : public QTabWidget
{
    Q_OBJECT
public:
    DockingTabWidget(QWidget *parent = nullptr);
    ~DockingTabWidget(void);

    //! Is the widget at this index docked or in an external dialog?
    bool isDocked(const int index) const;

    //! Get the currently selected tab widget
    QWidget *currentWidget(void) const;

    //! Get the widget at the specified index
    QWidget *widget(int index) const;

    //! Add a new page tab to the widget
    int addTab(QWidget *page, const QString &label);

    //! Add a new page tab at the specified index
    int insertTab(int index, QWidget *page, const QString &label);

    //! Set the tab text for the specified index
    void setTabText(int index, const QString &label);

    //! Get the tab text at the specified index
    QString tabText(int index) const;

private slots:
    void handleUndockButton(QWidget *);

protected:
    void tabInserted(int index);
    void tabRemoved(int index);

private:
    void internalUpdate(void);
    QSignalMapper *_mapper;
};
