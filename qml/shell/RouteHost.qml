import QtQuick
import QtQuick.Controls
import KmxBank

// The page stack both shells drive. Owning the factories here means a new page
// is wired once, not once per shell; the shells keep only their chrome.
Item {
    id: host

    required property var bank

    // Seeds the ledger filter when the user searches from a shell header.
    property string pendingLedgerSearch: ""

    // A page asked to move elsewhere (quick actions, deep links).
    signal routeRequested(string key)

    readonly property int depth: pageStack.depth
    readonly property bool atRoot: pageStack.depth <= 1

    // Pushed detail pages name themselves (an account statement is not
    // "Accounts"); the mobile top bar prefers this over the route title.
    readonly property string currentTitle:
        pageStack.currentItem ? (pageStack.currentItem.title || "") : ""

    // Swapping a primary destination is a state change, not a journey; only
    // drilling into a detail page animates.
    function show(key) {
        const factory = _factoryFor(key)
        if (!factory)
            return false
        pageStack.replace(null, factory, { bank: host.bank }, StackView.Immediate)
        return true
    }

    function openAccountDetails(account_id) {
        pageStack.push(accountDetailsFactory,
                       { bank: host.bank, account_id: account_id })
    }

    function pop() {
        if (pageStack.depth > 1) {
            pageStack.pop()
            return true
        }
        return false
    }

    function popToRoot() {
        if (pageStack.depth > 1) {
            pageStack.pop(null)
            return true
        }
        return false
    }

    function _factoryFor(key) {
        switch (key) {
        case "dashboard":    return dashboardFactory
        case "accounts":     return accountsFactory
        case "transactions": return transactionsFactory
        case "payments":     return paymentsFactory
        case "exchange":     return exchangeFactory
        case "cards":        return cardsFactory
        case "analytics":    return analyticsFactory
        case "connections":  return connectionsFactory
        case "settings":     return settingsFactory
        }
        return null
    }

    StackView {
        id: pageStack
        anchors.fill: parent
        clip: true
    }

    Component {
        id: connectionsFactory
        ConnectionsPage { background: null }
    }

    Component {
        id: dashboardFactory
        DashboardPage {
            background: null
            onNewTransferRequested: host.routeRequested("payments")
            onExchangeRequested: host.routeRequested("exchange")
            onFreezeCardRequested: host.routeRequested("cards")
            onSeeAllAccountsRequested: host.routeRequested("accounts")
            onAccountSelected: (account_id) => host.openAccountDetails(account_id)
        }
    }

    Component {
        id: accountsFactory
        AccountsPage {
            background: null
            onAccountSelected: (account_id) => host.openAccountDetails(account_id)
        }
    }

    Component {
        id: settingsFactory
        SettingsPage { background: null }
    }

    Component {
        id: analyticsFactory
        AnalyticsPage { background: null }
    }

    Component {
        id: cardsFactory
        CardsPage { background: null }
    }

    Component {
        id: exchangeFactory
        ExchangeHubPage { background: null }
    }

    Component {
        id: paymentsFactory
        PaymentsPage { background: null }
    }

    Component {
        id: transactionsFactory
        TransactionsPage { background: null; initialSearch: host.pendingLedgerSearch }
    }

    Component {
        id: accountDetailsFactory
        AccountDetailsPage {
            background: null
            onBackRequested: host.pop()
        }
    }
}
