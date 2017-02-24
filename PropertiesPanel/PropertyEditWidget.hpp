// Copyright (c) 2015-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <QWidget>
#include <QString>
#include <QPointer>
#include <QJsonObject>
#include <string>

class QLabel;
class QTimer;
class QToolButton;
class QVBoxLayout;
class QHBoxLayout;

/*!
 * The property edit widget creates an entry widget through a JSON description.
 * Possible edit widgets are arbitrary and found in the plugin registry.
 * This is a wrapper around an edit widget that handles evaluation results.
 */
class PropertyEditWidget : public QWidget
{
    Q_OBJECT
public:

    /*!
     * Make a new edit widget from a JSON description.
     * The initial value allows the widget to determine if a change occurred.
     */
    PropertyEditWidget(const QString &initialValue, const QJsonObject &paramDesc, const QString &editMode, QWidget *parent);

    ~PropertyEditWidget(void);

    //! Reload internal edit widget from a param desc change
    void reloadParamDesc(const QJsonObject &paramDesc);

    //! Get the initial value of the edit widget
    const QString &initialValue(void) const;

    //! Does this edit widget have a change vs the original value?
    bool changed(void) const;

    //! get the value of the edit widget
    QString value(void) const;

    //! set the value of the edit widget
    void setValue(const QString &value);

    //! Set the type string from an evaluation
    void setTypeStr(const QString &typeStr);

    //! Set the error message from an evaluation
    void setErrorMsg(const QString &errorMsg);

    //! Set the background color of the edit widget
    void setBackgroundColor(const QColor &color);

    //! Original edit mode applied to this widget
    const QString &initialEditMode(void) const;

    //! get the current edit mode
    QString editMode(void) const;

    /*!
     * Make a label that will track the widget's status.
     * This label might be used in a QFormLayout for example.
     * Label ownership goes to the caller.
     */
    QLabel *makeFormLabel(const QString &text, QWidget *parent);

    //! Cancel user-entry events - cancel pending timer signals
    void cancelEvents(void);

    //! Flush user-entry events - triggers pending timer signals
    void flushEvents(void);

signals:

    //! user pressed enter in an entry box -- force properties commit
    void commitRequested(void);

    //! any kind of immediate widget change other than text entry
    void widgetChanged(void);

    //! a text entry event from the user occurred
    void entryChanged(void);

private slots:
    void handleWidgetChanged(void);
    void handleEntryChanged(void);
    void handleCommitRequested(void);
    void handleModeButtonClicked(void);

private:
    void updateInternals(void);
    const QString _initialValue;
    QWidget *_editWidget;
    QLabel *_errorLabel;
    QPointer<QLabel> _formLabel;
    QString _formLabelText;
    QString _errorMsg;
    QString _unitsStr;
    QTimer *_entryTimer;
    QVBoxLayout *_editLayout;
    QToolButton *_modeButton;
    QHBoxLayout *_modeLayout;
    QWidget *_editParent;
    QColor _bgColor;
    QString _initialEditMode;
    QString _editMode;
    QJsonObject _lastParamDesc;
};
