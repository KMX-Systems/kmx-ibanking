/// @file src/app/ui_config.h
/// @brief Start-up selection between the desktop and mobile shells.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QObject>
    #include <QString>
    #include <QVariantList>
    #include <QVector>
#endif

class QCoreApplication;

namespace kmx
{

    /// @brief One entry of the mobile device preset table.
    /// @details Panels are described by their *physical* pixel grid and pixel density, the way vendors publish them. Qt lays out in
    /// logical pixels, so every layout decision downstream uses @ref logical_width / @ref logical_height, which are the physical
    /// dimensions divided by @ref device_pixel_ratio. A 1440 px wide QHD+ panel is a 411 px wide canvas as far as QML is concerned.
    struct device_preset
    {
        QString key;                 ///< Value accepted by --device.
        QString label;               ///< Human-readable name shown in Settings.
        QString tier;                ///< Market tier: ultra / flagship / sweetspot / budget.
        int physical_width {};       ///< Panel width in device pixels.
        int physical_height {};      ///< Panel height in device pixels.
        int ppi {};                  ///< Approximate pixel density.
        qreal device_pixel_ratio {}; ///< Logical-to-physical scale the platform reports.

        int logical_width() const { return qRound(physical_width / device_pixel_ratio); }
        int logical_height() const { return qRound(physical_height / device_pixel_ratio); }
    };

    /// @brief Resolved UI mode plus the emulated device geometry, exposed to QML as the `UiConfig` singleton.
    /// @details Resolution order is CLI flag, then environment variable, then the remembered QSettings value, then auto-detection
    /// from the platform and primary screen. Nothing here mutates after start-up, so every property is CONSTANT.
    class ui_config: public QObject
    {
        Q_OBJECT
        Q_PROPERTY(bool mobile READ mobile CONSTANT)
        Q_PROPERTY(bool emulated READ emulated CONSTANT)
        Q_PROPERTY(QString device_key READ device_key CONSTANT)
        Q_PROPERTY(QString device_label READ device_label CONSTANT)
        Q_PROPERTY(QString device_tier READ device_tier CONSTANT)
        Q_PROPERTY(int logical_width READ logical_width CONSTANT)
        Q_PROPERTY(int logical_height READ logical_height CONSTANT)
        Q_PROPERTY(int physical_width READ physical_width CONSTANT)
        Q_PROPERTY(int physical_height READ physical_height CONSTANT)
        Q_PROPERTY(int ppi READ ppi CONSTANT)
        Q_PROPERTY(qreal device_pixel_ratio READ device_pixel_ratio CONSTANT)
        Q_PROPERTY(QVariantList devices READ devices CONSTANT)
    public:
        explicit ui_config(QObject* parent = nullptr);

        /// @brief Applies --ui / --device (and their KMX_UI / KMX_DEVICE fallbacks), then persists the outcome.
        /// @details Exits the process after printing when --list-devices is passed.
        void resolve(const QCoreApplication& app);

        bool mobile() const { return mobile_; }
        /// @brief True when the mobile shell runs on a desktop screen, i.e. the window stands in for a handset.
        bool emulated() const { return mobile_ && !native_mobile_platform(); }

        QString device_key() const { return device().key; }
        QString device_label() const { return device().label; }
        QString device_tier() const { return device().tier; }
        int logical_width() const { return device().logical_width(); }
        int logical_height() const { return device().logical_height(); }
        int physical_width() const { return device().physical_width; }
        int physical_height() const { return device().physical_height; }
        int ppi() const { return device().ppi; }
        qreal device_pixel_ratio() const { return device().device_pixel_ratio; }

        /// @brief The preset table as QML-friendly maps, for the device picker in Settings.
        QVariantList devices() const;

        /// @brief Records the shell to start next time ("desktop", "mobile" or "auto").
        /// @details Deliberately does not restyle the running session: swapping shells mid-flight would tear down the page the user
        /// is standing on. The QML picker says as much.
        Q_INVOKABLE void set_preferred_mode(const QString& mode);

        /// @brief Records the device preset to emulate next time; unknown keys are ignored.
        Q_INVOKABLE void set_preferred_device(const QString& key);

        /// @brief Index of @p key in @ref presets, or -1.
        Q_INVOKABLE static int device_index_of(const QString& key);

        /// @brief The preset table; ordered ultra -> budget so the picker reads top-down.
        static const QVector<device_preset>& presets();

        /// @brief True on platforms whose real screen already is a handset, where presets are preview-only.
        static bool native_mobile_platform();

    private:
        const device_preset& device() const;

        /// @brief Picks a mode when no flag, variable or stored preference says otherwise.
        static bool detect_mobile();

        bool mobile_ {};
        int device_index_ {};
    };

}
