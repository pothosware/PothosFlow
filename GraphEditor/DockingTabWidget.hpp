// Copyright (c) 2018-2021 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <QTabWidget>

/*!
 * A customized tab widget with detachable tabs that can become independent windows.
 */
class DockingTabWidget : public QTabWidget
{
    Q_OBJECT
public:
    DockingTabWidget(QWidget *parent = nullptr);
    ~DockingTabWidget(void);

    //! Is this widget active in the main window (or one of its dialogs active)
    bool isActive(void) const;

    //! remove tabs, but do not delete widgets
    void clear(void);

    /*!
     * Get the active page index.
     * This is the page that has mouse focus,
     * which may be a dialog when undocked.
     */
    int activeIndex(void) const;

    //! Set the modified property (used for window title)
    void setWindowModified(const bool modified);

    //! The title for the dialog box
    void setWindowTitle(const QString &title);

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

    //! Is the widget at this index docked or in an external dialog?
    bool isDocked(int index) const;

    //! Set the dock state of the specified index
    void setDocked(int index, const bool docked);

    /*!
     * Save the dialog geometry of the specified page.
     * Even when a page is docked, it may have previous geometry to save.
     */
    QByteArray saveGeometry(int index) const;

    /*!
     * Restore the dialog geometry of the specified page.
     */
    bool restoreGeometry(int index, const QByteArray &geometry);

    //! Save the complete widget state (dialogs, geometry, selected index)
    QVariant saveWidgetState(void) const;

    //! Restore the widget state from saved state (return true for success)
    void restoreWidgetState(const QVariant &state);

signals:
    //! Called when active window changes (dialog vs main window focus)
    void activeChanged(void);

protected:
    void tabInserted(int index);
    void tabRemoved(int index);
    bool eventFilter(QObject *object, QEvent *event);

private:
    void handleUndockButton(QWidget *);
    class DockingPage;
    DockingPage *page(const int index) const;
    void internalUpdate(void);
    QString _dialogTitle;
};
