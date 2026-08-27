import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtCore
import KmxBank

// KMX shell: sidebar + header + StackView routing over the banking pages.
// Replaces the legacy tabbed Main.qml (plan §Phase 1).
ApplicationWindow {
    id: shell

    required property var bank      // kmx::BankingSession facade

    readonly property var routes: [
        { key: "dashboard",    title: qsTr("Dashboard"),    icon: "grid",},
        { key: "accounts",     title: qsTr("Accounts"),     icon: "landmark",},
        { key: "transactions", title: qsTr("Transactions"), icon: "receipt",},
        { key: "payments",     title: qsTr("Payments"),     icon: "swap",},
        { key: "exchange",     title: qsTr("Exchange"),     icon: "trend",},
        { key: "cards",        title: qsTr("Cards"),        icon: "card",},
        { key: "analytics",    title: qsTr("Analytics"),    icon: "pie",},
        { key: "connections",  title: qsTr("Connections"),  icon: "plug",},
        { key: "settings",     title: qsTr("Settings"),     icon: "sliders",}
    ]
    property int routeIndex: 0
    property string pendingLedgerSearch: ""

    // Stable access point for nested components (drawer deep links).
    readonly property var shellRoute: shell

    function open_route(key) {
        for (var i = 0; i < routes.length; ++i)
            if (routes[i].key === key) { routeIndex = i; return true }
        return false
    }

    function _showCurrent() {
        router.show(routes[routeIndex].key)
    }

    function openAccountDetails(account_id) {
        router.openAccountDetails(account_id)
    }

    onRouteIndexChanged: _showCurrent()

    // Persisted theme preference (org/app name come from main.cpp).
    Settings {
        id: uiSettings
        category: "ui"
        property string mode: "dark"
    }
    Connections {
        target: Theme
        function onModeChanged() { uiSettings.mode = Theme.mode }
    }

    function _onCompleted() {
        if (uiSettings.mode === "light" || uiSettings.mode === "dark")
            Theme.mode = uiSettings.mode
        _showCurrent()
    }
    Component.onCompleted: _onCompleted()

    width: 1680
    height: 920
    minimumWidth: 1120
    minimumHeight: 720
    visible: true
    title: qsTr("KMX")
    color: Theme.bg
    font.family: Theme.fontFamily

    // Qt Quick Controls' Material style keeps its own palette, so it has to
    // follow Theme.mode: left on Dark, every control painted light-on-light
    // (invisible button labels and outlines) once the app switched to light.
    Material.theme: Theme.isDark ? Material.Dark : Material.Light
    Material.accent: Theme.accent

    // Pages read FormFactor, not shell.width, so a narrow desktop window and a
    // handset reflow identically.
    Binding { target: FormFactor; property: "windowWidth";  value: shell.width - sidebar.currentWidth }
    Binding { target: FormFactor; property: "windowHeight"; value: shell.height }

    readonly property bool sessionActive:
        bank && bank.auth && bank.auth.state === AuthService.Active

    property bool _primaryLinked: false
    onSessionActiveChanged: {
        if (sessionActive && !_primaryLinked) {
            _primaryLinked = true
            bank.auto_connect_primary_bank() // your own bank is already linked
            shellReveal.restart()
            toasts.notify("success", qsTr("Welcome back, %1!").arg(bank.auth.display_name))
        } else if (sessionActive) {
            shellReveal.restart()
        }
    }

    // ---- authenticated shell ---------------------------------------------
    Item {
        id: appContent
        anchors.fill: parent
        visible: shell.sessionActive
        opacity: shell.sessionActive ? 1 : 0
        scale: shell.sessionActive ? 1 : 0.98

        NumberAnimation on scale { id: shellReveal; from: 0.97; to: 1;
                                   duration: 350; easing.type: Easing.OutCubic }

    SideBar {
            id: sidebar
            z: 10
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            model: shell.routes
            currentIndex: shell.routeIndex
                footerText: shell.bank ? shell.bank.status_line : ""
            onNavigate: (index) => shell.routeIndex = index
        }

        ColumnLayout {
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: sidebar.right
            anchors.right: parent.right
            spacing: 0

            AppHeader {
                Layout.fillWidth: true
                pageTitle: shell.routes[shell.routeIndex].title
                sidebarCollapsed: sidebar.collapsed
                unread_count: shell.bank ? shell.bank.notifications.unread_count : 0

                onToggleSidebar: sidebar.toggle()
                onSearchRequested: function (text) {
                    pendingLedgerSearch = text
                    shell.open_route("transactions")
                }
                onBellClicked: notifDrawer.open()
                onLockRequested: if (shell.bank) shell.bank.auth.lock()
                onLogoutRequested: if (shell.bank) shell.bank.auth.logout()
                onProfileRequested: shell.open_route("settings")
            }

            RouteHost {
                id: router
                bank: shell.bank
                pendingLedgerSearch: shell.pendingLedgerSearch
                Layout.fillWidth: true
                Layout.fillHeight: true

                onRouteRequested: (key) => shell.open_route(key)
            }
        }
    }

    // ---- auth gate layers --------------------------------------------------
    LoginPage {
        anchors.fill: parent
        visible: bank && bank.auth && bank.auth.state !== AuthService.Active
                 && bank.auth.state !== AuthService.Locked
        auth: bank ? bank.auth : null
    }

    LockOverlay {
        anchors.fill: parent
        z: 800
        visible: bank && bank.auth && bank.auth.state === AuthService.Locked
        auth: bank ? bank.auth : null
    }

    NotificationDrawer {
        id: notifDrawer
        z: 850
        bank: shell.bank
    }

    // ---- floating demo menu (plan §Phase 10) --------------------------------
    RoundButton {
        id: demoButton
        z: 880
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.spacingL
        width: 52; height: 52
        visible: shell.sessionActive
        text: qsTr("Demo")
        font.pixelSize: Theme.fontSizeCaption
        font.bold: true
        // Solid accent chip: the default button color would sit invisibly on
        // the light page, and the label must contrast with the accent itself.
        Material.background: Theme.accent
        Material.foreground: Theme.accentText
        onClicked: demoMenu.popup(demoButton.x, demoButton.y - demoMenu.height - 8)

        Menu {
            id: demoMenu
            width: 260

            MenuItem { text: qsTr("Simulate incoming salary"); onTriggered:
                shell.bank.simulate_incoming_salary() }
            MenuItem { text: qsTr("Simulate fraud alert"); onTriggered:
                shell.bank.simulate_fraud_alert() }
            MenuItem { text: qsTr("Simulate new-device login"); onTriggered:
                shell.bank.simulate_new_device_login() }
            MenuSeparator {}
            MenuItem { text: qsTr("FX market shock"); onTriggered:
                shell.bank.fx_shock() }
            MenuItem { text: qsTr("Expire BT session now"); onTriggered:
                shell.bank.force_bt_session_expiry() }
            MenuItem { text: qsTr("Run due standing orders"); onTriggered: {
                const n = shell.bank.payments.run_due_now(true)
                toasts.notify(n > 0 ? "success" : "info",
                              n > 0 ? qsTr("%1 order(s) executed").arg(n)
                                    : qsTr("Nothing due"))
            } }
        }
    }

    ToastHost { id: toasts; z: 900 }

    // Service notifications surface as toasts; deep links route the user.
    Connections {
        target: shell.bank ? shell.bank.notifications : null
        function onPosted(id, level, title, body, deep_link_key) {
            const map = { "warning": "warning", "critical": "critical",
                          "success": "success", "info": "info" }
            toasts.notify(map[level] || "info", title + (body ? " — " + body : ""),
                          deep_link_key ? qsTr("Review") : "",
                          deep_link_key ? function() { open_route(deep_link_key) } : null)
        }
    }

    // Idle auto-lock (plan §4): 5 minutes without input while active.
    Timer {
        interval: 1000
        repeat: true
        running: shell.sessionActive
        onTriggered: {
            if (bank.activity.seconds_idle >= bank.auth.idle_lock_seconds)
                bank.auth.lock()
        }
    }

    Shortcut { sequence: "Ctrl+1"; onActivated: shell.routeIndex = 0 }
    Shortcut { sequence: "Ctrl+2"; onActivated: shell.routeIndex = 1 }
    Shortcut { sequence: "Ctrl+3"; onActivated: shell.routeIndex = 2 }
    Shortcut { sequence: "Ctrl+4"; onActivated: shell.routeIndex = 3 }
    Shortcut { sequence: "Ctrl+5"; onActivated: shell.routeIndex = 4 }
    Shortcut { sequence: "Ctrl+6"; onActivated: shell.routeIndex = 5 }
    Shortcut { sequence: "Ctrl+7"; onActivated: shell.routeIndex = 6 }
    Shortcut { sequence: "Ctrl+8"; onActivated: shell.routeIndex = 7 }
    Shortcut { sequence: "Ctrl+9"; onActivated: shell.routeIndex = 8 }
    Shortcut { sequence: "Esc"; onActivated: router.pop() }
    Shortcut { sequence: "Ctrl+L"; onActivated:
        if (shell.sessionActive) bank.auth.lock() }
}
