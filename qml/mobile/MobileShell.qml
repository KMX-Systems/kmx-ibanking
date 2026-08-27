import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Window
import QtCore
import KmxBank

// Handset shell: top bar + full-bleed page stack + bottom navigation.
// Same routes, same pages and the same BankingSession as MainShell — only the
// chrome differs. main.cpp picks between the two at start-up (see ui_config).
ApplicationWindow {
    id: shell

    required property var bank      // kmx::banking_session facade

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

    // Four destinations in the nav bar; the rest reach the user via MoreSheet.
    readonly property var navKeys: ["dashboard", "accounts", "payments", "transactions"]

    function _entries(keys) {
        return routes.filter(function (r) { return keys.indexOf(r.key) >= 0 })
    }
    readonly property var navEntries: _entries(navKeys)
    readonly property var moreEntries:
        routes.filter(function (r) { return shell.navKeys.indexOf(r.key) < 0 })

    property int routeIndex: 0
    readonly property string currentKey: routes[routeIndex].key

    // Same entry point the desktop shell exposes; the CI route walk calls it.
    function open_route(key) {
        for (var i = 0; i < routes.length; ++i)
            if (routes[i].key === key) {
                if (routeIndex === i)
                    router.popToRoot()    // re-tapping a tab unwinds its detail pages
                else
                    routeIndex = i
                return true
            }
        return false
    }

    // Read routes[routeIndex] rather than currentKey: the change handler and the
    // currentKey binding both wake on routeIndex, and the handler can run first.
    onRouteIndexChanged: router.show(routes[routeIndex].key)

    // ---- window geometry ---------------------------------------------------
    // On a handset the real screen wins. On a desktop the window stands in for
    // one, sized to the selected preset's *logical* dimensions (UiConfig divides
    // the physical panel by its device pixel ratio).
    readonly property int shellWidth: UiConfig.emulated ? UiConfig.logical_width : Screen.width
    readonly property int shellHeight: UiConfig.emulated ? UiConfig.logical_height : Screen.height

    width: shellWidth
    height: shellHeight
    // Only ask for full screen where there is no window manager to negotiate
    // with. Forcing Windowed is not the same as leaving the platform default:
    // it makes some compositors re-map the window after it is first shown.
    visibility: UiConfig.emulated ? Window.AutomaticVisibility : Window.FullScreen

    // A handset has no resize handle, so neither does the window standing in for
    // one: pinning minimum == maximum makes the window manager drop it, and the
    // preview stays honest about the canvas the layouts were checked against.
    // Left unpinned on a real device, where the platform owns the geometry.
    minimumWidth: UiConfig.emulated ? shellWidth : 320
    maximumWidth: UiConfig.emulated ? shellWidth : sizeUnbounded
    minimumHeight: UiConfig.emulated ? shellHeight : 480
    maximumHeight: UiConfig.emulated ? shellHeight : sizeUnbounded

    // Qt's own "no maximum" sentinel; naming it keeps the bindings above honest.
    readonly property int sizeUnbounded: 16777215

    visible: true
    title: UiConfig.emulated
           ? qsTr("KMX — %1").arg(UiConfig.device_label)
           : qsTr("KMX")
    color: Theme.bg
    font.family: Theme.fontFamily

    Material.theme: Theme.isDark ? Material.Dark : Material.Light
    Material.accent: Theme.accent

    // Pages branch on FormFactor, never on shell.width, so a resized preview
    // window reflows exactly like a narrower handset would.
    Binding { target: FormFactor; property: "windowWidth";  value: shell.width }
    Binding { target: FormFactor; property: "windowHeight"; value: shell.height }
    Binding { target: FormFactor; property: "mobileShell";  value: true }

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
        router.show(currentKey)
    }
    Component.onCompleted: _onCompleted()

    readonly property bool sessionActive:
        bank && bank.auth && bank.auth.state === AuthService.Active

    property bool _primaryLinked: false
    onSessionActiveChanged: {
        if (sessionActive && !_primaryLinked) {
            _primaryLinked = true
            bank.auto_connect_primary_bank()
            toasts.notify("success", qsTr("Welcome back, %1!").arg(bank.auth.display_name))
        }
    }

    // ---- authenticated shell ----------------------------------------------
    Item {
        id: appContent
        anchors.fill: parent
        visible: shell.sessionActive

        MobileTopBar {
            id: topBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            pageTitle: router.currentTitle.length > 0
                       ? router.currentTitle
                       : shell.routes[shell.routeIndex].title
            unread_count: shell.bank ? shell.bank.notifications.unread_count : 0
            canGoBack: !router.atRoot

            onBackRequested: router.pop()
            onBellClicked: notifDrawer.open()
            onSearchRequested: function (text) {
                router.pendingLedgerSearch = text
                shell.open_route("transactions")
            }
        }

        RouteHost {
            id: router
            bank: shell.bank
            anchors.top: topBar.bottom
            anchors.bottom: navBar.top
            anchors.left: parent.left
            anchors.right: parent.right

            onRouteRequested: (key) => shell.open_route(key)
        }

        MobileNav {
            id: navBar
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            model: shell.navEntries
            currentKey: shell.currentKey
            moreActive: shell.navKeys.indexOf(shell.currentKey) < 0
            unread_count: shell.bank ? shell.bank.notifications.unread_count : 0

            onNavigate: (key) => shell.open_route(key)
            onMoreRequested: moreSheet.open()
        }
    }

    MoreSheet {
        id: moreSheet
        entries: shell.moreEntries
        currentKey: shell.currentKey
        statusLine: shell.bank ? shell.bank.status_line : ""

        onNavigate: function (key) {
            close()
            shell.open_route(key)
        }
        onDemoRequested: demoMenu.popup()
        onLockRequested: if (shell.bank) shell.bank.auth.lock()
        onLogoutRequested: if (shell.bank) shell.bank.auth.logout()
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

    Menu {
        id: demoMenu
        width: Math.min(280, shell.width - Theme.spacingXL)

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

    ToastHost { id: toasts; z: 900; bottomInset: navBar.height }

    Connections {
        target: shell.bank ? shell.bank.notifications : null
        function onPosted(id, level, title, body, deep_link_key) {
            const map = { "warning": "warning", "critical": "critical",
                          "success": "success", "info": "info" }
            toasts.notify(map[level] || "info", title + (body ? " — " + body : ""),
                          deep_link_key ? qsTr("Review") : "",
                          deep_link_key ? function() { shell.open_route(deep_link_key) } : null)
        }
    }

    Timer {
        interval: 1000
        repeat: true
        running: shell.sessionActive
        onTriggered: {
            if (bank.activity.seconds_idle >= bank.auth.idle_lock_seconds)
                bank.auth.lock()
        }
    }

    // Android's hardware/gesture back: unwind the stack, then the drawers.
    Shortcut {
        sequences: [StandardKey.Back, StandardKey.Cancel]
        onActivated: {
            if (moreSheet.opened) moreSheet.close()
            else if (notifDrawer.opened) notifDrawer.close()
            else router.pop()
        }
    }
}
