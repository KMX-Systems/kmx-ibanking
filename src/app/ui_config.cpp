/// @file src/app/ui_config.cpp
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "app/ui_config.h"
#ifndef PCH
    #include <QCommandLineParser>
    #include <QCoreApplication>
    #include <QGuiApplication>
    #include <QScreen>
    #include <QSettings>
    #include <QTextStream>
    #include <QVariantMap>
#endif

namespace kmx
{
    namespace
    {
        /// @brief Preset chosen when neither --device nor a stored preference names one.
        constexpr auto default_device_key = "flagship";

        /// @brief Below this logical width the desktop sidebar shell stops being usable.
        constexpr int handset_width_threshold = 600;

    }

    int ui_config::device_index_of(const QString& key)
    {
        const auto& table = presets();
        for (int i = 0; i < table.size(); ++i)
            if (table[i].key.compare(key, Qt::CaseInsensitive) == 0)
                return i;
        return -1;
    }

    const QVector<device_preset>& ui_config::presets()
    {
        // Physical grid + density per market tier; the logical canvas each one
        // yields is what the layouts actually see (see device_preset).
        //                key                 label                                       tier          px w   px h   ppi   dpr
        static const QVector<device_preset> table {
            {QStringLiteral("ultra"), QStringLiteral("Ultra flagship QHD+ · Galaxy S26 Ultra"), QStringLiteral("ultra"), 1440, 3120, 505,
             3.5},
            {QStringLiteral("ultra-xl"), QStringLiteral("Ultra flagship QHD+ tall · Pixel 10 Pro XL"), QStringLiteral("ultra"), 1440, 3200,
             520, 3.5},
            {QStringLiteral("flagship"), QStringLiteral("Mainstream flagship · iPhone 17 Pro"), QStringLiteral("flagship"), 1320, 2868, 460,
             3.0},
            {QStringLiteral("flagship-compact"), QStringLiteral("Mainstream flagship compact · iPhone 17"), QStringLiteral("flagship"),
             1284, 2778, 458, 3.0},
            {QStringLiteral("sweetspot"), QStringLiteral("Upper mid-range 1.5K · OnePlus / Nothing"), QStringLiteral("sweetspot"), 1240,
             2772, 450, 3.0},
            {QStringLiteral("sweetspot-wide"), QStringLiteral("Upper mid-range 1.5K · Honor / Xiaomi"), QStringLiteral("sweetspot"), 1264,
             2780, 460, 3.0},
            {QStringLiteral("budget"), QStringLiteral("Budget FHD+ · Galaxy A-series"), QStringLiteral("budget"), 1080, 2400, 395, 2.75},
            {QStringLiteral("budget-tall"), QStringLiteral("Budget FHD+ tall · Moto G"), QStringLiteral("budget"), 1080, 2412, 400, 2.75},
        };
        return table;
    }

    bool ui_config::native_mobile_platform()
    {
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
        return true;
#else
        return false;
#endif
    }

    ui_config::ui_config(QObject* parent): QObject(parent)
    {
        device_index_ = qMax(0, device_index_of(QString::fromLatin1(default_device_key)));
    }

    const device_preset& ui_config::device() const
    {
        return presets()[device_index_];
    }

    QVariantList ui_config::devices() const
    {
        QVariantList out;
        out.reserve(presets().size());
        for (const device_preset& d: presets())
        {
            out.append(QVariantMap {{QStringLiteral("key"), d.key},
                                    {QStringLiteral("label"), d.label},
                                    {QStringLiteral("tier"), d.tier},
                                    {QStringLiteral("physical_width"), d.physical_width},
                                    {QStringLiteral("physical_height"), d.physical_height},
                                    {QStringLiteral("ppi"), d.ppi},
                                    {QStringLiteral("device_pixel_ratio"), d.device_pixel_ratio},
                                    {QStringLiteral("logical_width"), d.logical_width()},
                                    {QStringLiteral("logical_height"), d.logical_height()}});
        }
        return out;
    }

    bool ui_config::detect_mobile()
    {
        if (native_mobile_platform())
            return true;

        // A desktop session can still be driven on a handset-sized screen (a
        // convertible, a scaled kiosk display); go by the logical canvas.
        if (const QScreen* screen = QGuiApplication::primaryScreen())
            return screen->availableGeometry().width() < handset_width_threshold;

        return false;
    }

    void ui_config::set_preferred_mode(const QString& mode)
    {
        const QString normalized = mode.trimmed().toLower();
        if (normalized != QLatin1String("desktop") && normalized != QLatin1String("mobile") && normalized != QLatin1String("auto"))
            return;

        QSettings settings;
        settings.setValue(QStringLiteral("ui/mode"), normalized);
    }

    void ui_config::set_preferred_device(const QString& key)
    {
        if (device_index_of(key) < 0)
            return;

        QSettings settings;
        settings.setValue(QStringLiteral("ui/device"), key);
    }

    void ui_config::resolve(const QCoreApplication& app)
    {
        QCommandLineParser parser;
        parser.setApplicationDescription(QStringLiteral("KMX multi-bank client"));
        const QCommandLineOption help = parser.addHelpOption();
        parser.addVersionOption();

        const QCommandLineOption ui_option({QStringLiteral("ui")},
                                           QStringLiteral("Shell to start: desktop, mobile or auto (default: last used)."),
                                           QStringLiteral("mode"));
        const QCommandLineOption device_option(
            {QStringLiteral("device")}, QStringLiteral("Mobile device preset to emulate; see --list-devices."), QStringLiteral("key"));
        const QCommandLineOption list_option({QStringLiteral("list-devices")}, QStringLiteral("Print the mobile device presets and exit."));
        parser.addOption(ui_option);
        parser.addOption(device_option);
        parser.addOption(list_option);
        // Unknown options are ignored rather than fatal: Qt platform plugins add
        // their own (-platform, -style) and the app must still start.
        parser.parse(app.arguments());
        if (parser.isSet(help))
            parser.showHelp(0);

        if (parser.isSet(list_option))
        {
            QTextStream out(stdout);
            out << "key                tier        physical      ppi   dpr    logical\n";
            for (const device_preset& d: presets())
            {
                out << QStringLiteral("%1 %2 %3 %4 %5 %6\n")
                           .arg(d.key, -18)
                           .arg(d.tier, -11)
                           .arg(QStringLiteral("%1x%2").arg(d.physical_width).arg(d.physical_height), -13)
                           .arg(d.ppi, -5)
                           .arg(d.device_pixel_ratio, -6, 'g', 3)
                           .arg(QStringLiteral("%1x%2").arg(d.logical_width()).arg(d.logical_height()));
            }
            out.flush();
            ::exit(0);
        }

        QSettings settings; // org/app name are set by main() before this runs.

        // --- mode: CLI > env > remembered > auto ------------------------------
        // Only an explicit choice is remembered. Persisting an auto-detected one
        // would freeze the first run's guess forever, and would make a one-off
        // --ui=mobile silently sticky for every later launch.
        QString mode = parser.value(ui_option).trimmed().toLower();
        if (mode.isEmpty())
            mode = qEnvironmentVariable("KMX_UI").trimmed().toLower();
        const bool mode_is_explicit = !mode.isEmpty();
        if (mode.isEmpty())
            mode = settings.value(QStringLiteral("ui/mode")).toString().trimmed().toLower();

        if (mode == QLatin1String("mobile"))
            mobile_ = true;
        else if (mode == QLatin1String("desktop"))
            mobile_ = false;
        else // "auto", empty, or an unrecognised value
            mobile_ = detect_mobile();

        // --- device: CLI > env > remembered > default -------------------------
        QString device = parser.value(device_option).trimmed();
        if (device.isEmpty())
            device = qEnvironmentVariable("KMX_DEVICE").trimmed();
        const bool device_is_explicit = !device.isEmpty();
        if (device.isEmpty())
            device = settings.value(QStringLiteral("ui/device")).toString().trimmed();

        if (const int found = device_index_of(device); found >= 0)
            device_index_ = found;

        if (mode_is_explicit)
            settings.setValue(QStringLiteral("ui/mode"), mobile_ ? QStringLiteral("mobile") : QStringLiteral("desktop"));
        if (device_is_explicit)
            settings.setValue(QStringLiteral("ui/device"), device_key());
    }

}
