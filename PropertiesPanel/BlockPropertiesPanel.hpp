// Copyright (c) 2014-2021 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <QWidget>
#include <QString>
#include <QJsonObject>
#include "GraphEditor/GraphState.hpp"
#include "GraphObjects/GraphBlock.hpp"
#include <map>

class GraphBlock;
class QLabel;
class QLineEdit;
class QComboBox;
class QFormLayout;
class QTabWidget;
class PropertyEditWidget;

class BlockPropertiesPanel : public QWidget
{
    Q_OBJECT
public:
    BlockPropertiesPanel(GraphBlock *block, QWidget *parent);

    QString getParamDocString(const QString &propKey);

public slots:
    void handleCancel(void);
    void handleCommit(void);

private slots:

    void handleBlockDestroyed(QObject *);

    //! Handle for all widget change events
    void handleWidgetChanged(void);

    void handleAffinityZoneChanged(void);

    void handleBlockEvalDone(void);

    void handleParamDescChanged(const QString &, const QJsonObject &);

    void handleDocTabChanged(int);

signals:
    void stateChanged(const GraphState &);

private:

    std::map<QString, PropertyEditWidget *> _propIdToEditWidget;

    /*!
     * Update everything in this panel after a block change
     */
    void updateAllForms(void);

    /*!
     * Update all the things that change when a property is modified.
     * Label string formatting, color of the box, tooltip...
     */
    void updatePropForms(const QString &propKey);

    bool _ignoreChanges;

    PropertyEditWidget *_idLineEdit;

    QString _affinityZoneOriginal;
    QLabel *_affinityZoneLabel;
    QComboBox *_affinityZoneBox;

    QLabel *_blockErrorLabel;
    QTabWidget *_infoTabs;
    QLabel *_blockInfoDesc;
    QLabel *_jsonBlockDesc;
    QLabel *_evalTypesDesc;
    QFormLayout *_formLayout;
    std::map<QString, QFormLayout *> _paramLayouts;
    QTabWidget *_propertiesTabs;
    std::map<QWidget *, QString> _tabWidgetToTabName;
    QPointer<GraphBlock> _block;
};
