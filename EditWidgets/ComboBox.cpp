// Copyright (c) 2014-2021 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Plugin.hpp>
#include <QJsonObject>
#include <QJsonArray>
#include <QAbstractItemView>
#include <QComboBox>
#include <QLineEdit>

/***********************************************************************
 * ComboBox for drop-down entry
 **********************************************************************/
class ComboBox : public QComboBox
{
    Q_OBJECT
public:
    ComboBox(QWidget *parent):
        QComboBox(parent),
        _nonEditCount(0)
    {
        connect(this, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ComboBox::handleWidgetChanged);
        connect(this, &QComboBox::editTextChanged, this, &ComboBox::handleEntryChanged);
        this->view()->setObjectName("BlockPropertiesEditWidget"); //to pick up eval color style
    }

    void setEditable(const bool enable)
    {
        QComboBox::setEditable(enable);
        //line-edit should be non-null when enabled
        if (this->lineEdit() != nullptr)
        {
            connect(this->lineEdit(), &QLineEdit::returnPressed, this, &ComboBox::commitRequested);
        }
    }

    void doneAddingItems(void)
    {
        //count may change when line edit is used, we need to detect this
        _nonEditCount = this->count();
    }

public slots:
    QString value(void) const
    {
        return _value;
    }

    void setValue(const QString &value)
    {
        _value = value;
        int index = -1;
        for (int i = 0; i < QComboBox::count(); i++)
        {
            if (QComboBox::itemData(i).toString() == value) index = i;
        }
        if (index < 0) QComboBox::setEditText(value);
        else QComboBox::setCurrentIndex(index);
    }

signals:
    void commitRequested(void);
    void widgetChanged(void);
    void entryChanged(void);

private slots:
    void handleWidgetChanged(const int index)
    {
        const auto item = QComboBox::itemData(index);
        if (item.isValid()) _value = item.toString();

        //when returnPressed() occurs for the lineEdit() widget
        //this hook gets called instead with the index being
        //the maximum value which maps to the line edit widget
        if (this->lineEdit() != nullptr and index == _nonEditCount)
        {
            _value = this->lineEdit()->text();
        }

        emit this->widgetChanged();
    }
    void handleEntryChanged(const QString &val)
    {
        _value = val;
        emit this->entryChanged();
    }

private:
    QString _value;
    int _nonEditCount;
};

/***********************************************************************
 * Factory function and registration
 **********************************************************************/
static QWidget *makeComboBox(const QJsonArray &args, const QJsonObject &kwargs, QWidget *parent)
{
    auto comboBox = new ComboBox(parent);
    comboBox->setEditable(kwargs["editable"].toBool(false));
    for (const auto &optionVal : args)
    {
        const auto option = optionVal.toObject();
        comboBox->addItem(option["name"].toString(), option["value"].toString());
    }
    comboBox->doneAddingItems();
    return comboBox;
}

pothos_static_block(registerComboBox)
{
    Pothos::PluginRegistry::add("/flow/EntryWidgets/ComboBox", Pothos::Callable(&makeComboBox));
}

#include "ComboBox.moc"
