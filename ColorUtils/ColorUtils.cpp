// Copyright (c) 2013-2021 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "ColorUtils/ColorUtils.hpp"
#include <Pothos/Framework.hpp>
#include <Pothos/Util/TypeInfo.hpp>
#include <QPixmap>
#include <QCryptographicHash>
#include <QReadWriteLock>
#include <type_traits>
#include <complex>
#include <cstdint>

bool isColorDark(const QColor &color)
{
    return color.lightnessF() < 0.5;
}

/***********************************************************************
 * color map helper utilities
 **********************************************************************/
static QColor pastelize(const QColor &c)
{
    //Pastels have high value and low to intermediate saturation:
    //http://en.wikipedia.org/wiki/Pastel_%28color%29
    return QColor::fromHsv(c.hue(), int(c.saturationF()*128), int(c.valueF()*64)+191);
}

static QColor __typeStrToColor(const QString &typeStr)
{
    //This first part does nothing more than create 3 random 8bit numbers
    //by mapping a chunk of a repeatable hash function to a color hex code.
    QCryptographicHash hasher(QCryptographicHash::Md5);
    hasher.addData(typeStr.toUtf8());
    const auto hexHash = hasher.result().toHex();
    QColor c("#" + QString(hexHash).mid(0, 6));

    //Use the 3 random numbers to create a pastel color.
    return pastelize(c);
}

/***********************************************************************
 * color map cache structures
 **********************************************************************/
struct ColorMap : std::map<QString, QColor>
{
    ColorMap(void);

    void registerName(const std::string &name, const QColor &color)
    {
        (*this)[QString::fromStdString(name)] = color;
    }

    template <typename Type>
    void registerDType(const QColor &color)
    {
        this->registerName(Pothos::DType(typeid(Type)).toString(), color);
    }

    template <typename Type>
    void registerIntType(const QColor &color)
    {
        this->registerDType<typename std::make_signed<Type>::type>(color);
        this->registerDType<std::complex<typename std::make_signed<Type>::type>>(color.darker());
        this->registerDType<typename std::make_unsigned<Type>::type>(color);
        this->registerDType<std::complex<typename std::make_unsigned<Type>::type>>(color.darker());
    }

    template <typename Type>
    void registerFloatType(const QColor &color)
    {
        this->registerDType<Type>(color);
        this->registerDType<std::complex<Type>>(color.darker());
    }

    template <typename Type>
    void registerRType(const QColor &color)
    {
        this->registerName(Pothos::Util::typeInfoToString(typeid(Type)), color);
    }
};

Q_GLOBAL_STATIC(QReadWriteLock, getLookupMutex)
Q_GLOBAL_STATIC(ColorMap, getColorMap)

/*!
 * Initialize color map with some predefined colors.
 * Useful colors: http://www.computerhope.com/htmcolor.htm
 */
ColorMap::ColorMap(void)
{
    //the unspecified type
    this->registerName(Pothos::DType().name(), Qt::gray);

    //integer types
    registerIntType<int8_t>(Qt::magenta);
    registerIntType<int16_t>(Qt::yellow);
    registerIntType<int32_t>(Qt::green);
    static const QColor orange("#FF7F00");
    registerIntType<int64_t>(orange);

    //floating point
    registerFloatType<float>(Qt::red);
    static const QColor skyBlue("#6698FF");
    registerFloatType<double>(skyBlue);

    //strings
    static const QColor cornYellow("#FFF380");
    registerRType<std::string>(cornYellow);
    registerRType<std::wstring>(cornYellow);
    registerRType<QString>(cornYellow);

    //boolean
    static const QColor tiffBlue("#81D8D0");
    registerRType<bool>(tiffBlue);

    //finalize with the pastelize
    for (auto &pair : *this) pair.second = pastelize(pair.second);
}

/***********************************************************************
 * color utils functions
 **********************************************************************/
QColor typeStrToColor(const QString &typeStr_)
{
    auto typeStr = typeStr_;
    if (typeStr.isEmpty()) return Qt::white; //not specified

    //try to pass it through DType to get a "nice" name
    try
    {
        Pothos::DType dtype(typeStr.toStdString());
        //use a darker color for multiple dimensions of primitive types
        if (dtype.dimension() > 1)
        {
            const auto name1d = Pothos::DType::fromDType(dtype, 1).toMarkup();
            return typeStrToColor(QString::fromStdString(name1d)).darker(120);
        }
        typeStr = QString::fromStdString(dtype.toMarkup());
    }
    catch (const Pothos::DTypeUnknownError &){}

    //check the cache
    {
        QReadLocker lock(getLookupMutex());
        auto it = getColorMap()->find(typeStr);
        if (it != getColorMap()->end()) return it->second;
    }

    //create a new entry
    const auto color = __typeStrToColor(typeStr);
    QWriteLocker lock(getLookupMutex());
    return getColorMap()->emplace(typeStr, color).first->second;
}

std::map<QString, QColor> getTypeStrToColorMap(void)
{
    QReadLocker lock(getLookupMutex());
    return *getColorMap();
}

QIcon colorToWidgetIcon(const QColor &color)
{
    QPixmap pixmap(10, 10);
    pixmap.fill(color);
    return QIcon(pixmap);
}
