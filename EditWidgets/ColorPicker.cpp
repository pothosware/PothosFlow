// Copyright (c) 2016-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Plugin.hpp>
#include <QJsonObject>
#include <QJsonArray>
#define QT_QTCOLORPICKER_IMPORT
#include <QtColorPicker>
#include <stdexcept>

/***********************************************************************
 * ColorPicker for integer entry
 **********************************************************************/
class ColorPicker : public QtColorPicker
{
    Q_OBJECT
public:
    ColorPicker(QWidget *parent, const QString &mode):
        QtColorPicker(parent),
        _value("black")
    {
        if (mode == "default") this->setStandardColors();
        else if (mode == "pastel")
        {
            //https://en.wikipedia.org/wiki/List_of_colors_%28compact%29
            this->insertColor(QColor(119,158,203), tr("Dark pastel blue"));
            this->insertColor(QColor(3,192,60), tr("Dark pastel green"));
            this->insertColor(QColor(150,111,214), tr("Dark pastel purple"));
            this->insertColor(QColor(194,59,34), tr("Dark pastel red"));
            this->insertColor(QColor(177,156,217), tr("Light pastel purple"));
            this->insertColor(QColor(174,198,207), tr("Pastel blue"));
            this->insertColor(QColor(130,105,83), tr("Pastel brown"));
            this->insertColor(QColor(207,207,196), tr("Pastel gray"));
            this->insertColor(QColor(119,221,119), tr("Pastel green"));
            this->insertColor(QColor(244,154,194), tr("Pastel magenta"));
            this->insertColor(QColor(255,179,71), tr("Pastel orange"));
            this->insertColor(QColor(222,165,164), tr("Pastel pink"));
            this->insertColor(QColor(179,158,181), tr("Pastel purple"));
            this->insertColor(QColor(255,105,97), tr("Pastel red"));
            this->insertColor(QColor(203,153,201), tr("Pastel violet"));
            this->insertColor(QColor(253,253,150), tr("Pastel yellow"));
        }
        else throw std::runtime_error("ColorPicker mode must be default or pastel");
        connect(this, SIGNAL(colorChanged(const QColor &)), this, SLOT(handleColorChanged(const QColor &)));
    }

public slots:
    QString value(void) const
    {
        return QString("\"%1\"").arg(_value);
    }

    void setValue(const QString &value)
    {
        if (_value.size() < 2) return;
        _value = value.midRef(1, value.size()-2).toString();
        QtColorPicker::setCurrentColor(_value);
    }

signals:
    void commitRequested(void);
    void widgetChanged(void);
    void entryChanged(void);

private slots:
    void handleColorChanged(const QColor &color)
    {
        _value = color.name();
        emit this->widgetChanged();
    }

private:
    QString _value;
};

/***********************************************************************
 * Factory function and registration
 **********************************************************************/
static QWidget *makeColorPicker(const QJsonArray &, const QJsonObject &kwargs, QWidget *parent)
{
    return new ColorPicker(parent, kwargs["mode"].toString("default"));
}

pothos_static_block(registerColorPicker)
{
    Pothos::PluginRegistry::add("/flow/EntryWidgets/ColorPicker", Pothos::Callable(&makeColorPicker));
}

#include "ColorPicker.moc"
