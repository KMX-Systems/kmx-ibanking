cmake_minimum_required(VERSION 3.21)

project(KmxBank VERSION 0.1 LANGUAGES CXX)

# Build at C++26 when the toolchain can (clang-20 in the dev image), otherwise
# fall back to C++23 so the stock GCC 13 in the base image still builds.
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-std=c++2c" KMX_HAS_CXX26)
if(KMX_HAS_CXX26)
    set(CMAKE_CXX_STANDARD 26)
else()
    set(CMAKE_CXX_STANDARD 23)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
message(STATUS "KmxBank: building with C++${CMAKE_CXX_STANDARD} (${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION})")

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Quick QuickControls2 LinguistTools)

qt_standard_project_setup(REQUIRES 6.11)
enable_testing()

# ---------------------------------------------------------------------------
# kmxcore: domain + connectors + services, shared by the app and the tests.
# ---------------------------------------------------------------------------
qt_add_library(kmxcore STATIC
    src/viewmodels/account_model.cpp
    src/viewmodels/card_model.cpp
    src/viewmodels/beneficiary_model.cpp
    src/viewmodels/banking_session.cpp
    src/viewmodels/recent_transactions_model.cpp
    src/viewmodels/transaction_filter_proxy.cpp
    src/domain/iban.cpp
    src/domain/seed_world.cpp
    src/connectors/kmx_connector.cpp
    src/connectors/seed_bank_connector.cpp
    src/services/account_service.cpp
    src/services/card_service.cpp
    src/services/analytics_service.cpp
    src/services/auth_service.cpp
    src/services/budget_service.cpp
    src/services/fx_service.cpp
    src/services/normalization.cpp
    src/services/exchange_advisor_service.cpp
    src/services/payment_service.cpp
    src/services/notification_service.cpp
    src/services/connection_service.cpp
    src/services/user_activity_monitor.cpp
)
target_include_directories(kmxcore PUBLIC src)
target_link_libraries(kmxcore PUBLIC Qt6::Core Qt6::Gui) # QClipboard + colors at boundary

set_source_files_properties(assets/logos/bt-logo.svg PROPERTIES QT_RESOURCE_ALIAS bt-logo.svg)
set_source_files_properties(assets/logos/tbi-logo.svg PROPERTIES QT_RESOURCE_ALIAS tbi-logo.svg)
set_source_files_properties(assets/logos/tbi-logo-white.svg PROPERTIES QT_RESOURCE_ALIAS tbi-logo-white.svg)
set_source_files_properties(assets/logos/erste-logo.svg PROPERTIES QT_RESOURCE_ALIAS erste-logo.svg)

qt_add_resources(kmxcore "bankLogos"
    PREFIX "/kmx/logos"
    FILES
        assets/logos/bt-logo.svg
        assets/logos/tbi-logo.svg
        assets/logos/tbi-logo-white.svg
        assets/logos/erste-logo.svg
)

add_subdirectory(tests)

# ---------------------------------------------------------------------------
# Application (legacy demo UI + new banking session skeleton coexist in P0).
# ---------------------------------------------------------------------------
qt_add_executable(appkmxbank
    src/main.cpp
    src/app/ui_config.cpp
)

# Singletons must be declared before qt_add_qml_module reads source properties.
set_source_files_properties(qml/theme/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(qml/theme/FormFactor.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(appkmxbank
    URI KmxBank
    VERSION 1.0
    QML_FILES
        qml/shell/MainShell.qml
        qml/shell/RouteHost.qml
        qml/shell/SideBar.qml
        qml/shell/AppHeader.qml
        qml/shell/NotificationDrawer.qml
        qml/components/BankBadge.qml
        qml/components/CurrencyLabel.qml
        qml/components/AnimatedBalance.qml
        qml/components/Sparkline.qml
        qml/components/SkeletonBox.qml
        qml/components/EmptyState.qml
        qml/components/SectionHeader.qml
        qml/components/CategoryChip.qml
        qml/components/DonutChart.qml
        qml/components/CashflowBars.qml
        qml/components/TrendChart.qml
        qml/components/ToastHost.qml
        qml/auth/LoginPage.qml
        qml/auth/OtpDialog.qml
        qml/auth/LockOverlay.qml
        qml/components/PinPad.qml
        qml/pages/AccountDetailsPage.qml
        qml/pages/AccountsPage.qml
        qml/pages/DashboardPage.qml
        qml/pages/TransactionsPage.qml
        qml/pages/AnalyticsPage.qml
        qml/pages/SettingsPage.qml
        qml/pages/CardsPage.qml
        qml/pages/ExchangeHubPage.qml
        qml/pages/PaymentsPage.qml
        qml/pages/ConnectionsPage.qml
        qml/pages/ConnectBankFlow.qml
        qml/mobile/MobileShell.qml
        qml/mobile/MobileTopBar.qml
        qml/mobile/MobileNav.qml
        qml/mobile/MoreSheet.qml
        qml/theme/Theme.qml
        qml/theme/FormFactor.qml
        qml/theme/Format.js
)

# Mount icon files flat under /kmx/icons (qt_add_resources keeps directories
# relative to the project root unless each file gets QT_RESOURCE_ALIAS).
set(KMX_APP_ICONS
    assets/icons/grid.svg
    assets/icons/landmark.svg
    assets/icons/receipt.svg
    assets/icons/swap.svg
    assets/icons/trend.svg
    assets/icons/card.svg
    assets/icons/pie.svg
    assets/icons/plug.svg
    assets/icons/sliders.svg
    assets/icons/search.svg
    assets/icons/bell.svg
    assets/icons/lock.svg
    assets/icons/user.svg
    assets/icons/menu.svg
    assets/icons/plus.svg
    assets/icons/x.svg
    assets/icons/inbox.svg
    assets/icons/chevleft.svg
    assets/icons/chevright.svg
)
foreach(icon_path ${KMX_APP_ICONS})
    get_filename_component(icon_name ${icon_path} NAME)
    set_source_files_properties(${icon_path} PROPERTIES QT_RESOURCE_ALIAS ${icon_name})
endforeach()

qt_add_resources(appkmxbank "appIcons"
    PREFIX "/kmx/icons"
    FILES ${KMX_APP_ICONS}
)

target_link_libraries(appkmxbank
    PRIVATE Qt6::Quick Qt6::QuickControls2 kmxcore
)

qt_add_translations(appkmxbank
    TS_FILES
        translations/kmxbank_en.ts
        translations/kmxbank_ro.ts
)

install(TARGETS appkmxbank
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
