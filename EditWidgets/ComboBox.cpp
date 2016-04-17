// Copyright (c) 2014-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Plugin.hpp>
#include <Poco/JSON/Object.h>
#include <QAbstractItemView>
#include <QComboBox>

/***********************************************************************
 * ComboBox for drop-down entry
 **********************************************************************/
class ComboBox : public QComboBox
{
    Q_OBJECT
public:
    ComboBox(QWidget *parent):
        QComboBox(parent)
    {
        connect(this, SIGNAL(currentIndexChanged(const QString &)), this, SLOT(handleWidgetChanged(const QString &)));
        connect(this, SIGNAL(editTextChanged(const QString &)), this, SLOT(handleEntryChanged(const QString &)));
        this->view()->setObjectName("BlockPropertiesEditWidget"); //to pick up eval color style
    }

public slots:
    QString value(void) const
    {
        const auto index = QComboBox::currentIndex();
        if (index < 0 or QComboBox::currentText() != QComboBox::itemText(index)) return QComboBox::currentText();
        else return QComboBox::itemData(index).toString();
    }

    void setValue(const QString &value)
    {
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
    void handleWidgetChanged(const QString &)
    {
        emit this->widgetChanged();
    }
    void handleEntryChanged(const QString &)
    {
        emit this->entryChanged();
    }
};

/***********************************************************************
 * Factory function and registration
 **********************************************************************/
static QWidget *makeComboBox(const Poco::JSON::Object::Ptr &paramDesc, QWidget *parent)
{
    Poco::JSON::Object::Ptr widgetKwargs(new Poco::JSON::Object());
    if (paramDesc->has("widgetKwargs")) widgetKwargs = paramDesc->getObject("widgetKwargs");

    auto comboBox = new ComboBox(parent);
    comboBox->setEditable(widgetKwargs->optValue<bool>("editable", false));
    if (paramDesc->isArray("options")) for (const auto &optionObj : *paramDesc->getArray("options"))
    {
        const auto option = optionObj.extract<Poco::JSON::Object::Ptr>();
        comboBox->addItem(
            QString::fromStdString(option->getValue<std::string>("name")),
            QString::fromStdString(option->getValue<std::string>("value")));
    }
    return comboBox;
}

pothos_static_block(registerComboBox)
{
    Pothos::PluginRegistry::add("/gui/EntryWidgets/ComboBox", Pothos::Callable(&makeComboBox));
}

#include "ComboBox.moc"
