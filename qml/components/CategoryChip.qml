import QtQuick
import KmxBank

// Colored chip for a transaction category. `category` mirrors kmx::TxnCategory.
Rectangle {
    id: root

    property int category: 13 // Other

    // Chained literals (not an array) so LanguageChange re-evaluates the
    // binding and the label follows a language switch.
    function label(category) {
        return category === 0  ? qsTr("Salary")
             : category === 1  ? qsTr("Groceries")
             : category === 2  ? qsTr("Dining")
             : category === 3  ? qsTr("Transport")
             : category === 4  ? qsTr("Utilities")
             : category === 5  ? qsTr("Shopping")
             : category === 6  ? qsTr("Health")
             : category === 7  ? qsTr("Entertainment")
             : category === 8  ? qsTr("Travel")
             : category === 9  ? qsTr("Fees")
             : category === 10 ? qsTr("Transfer")
             : category === 11 ? qsTr("Interest")
             : category === 12 ? qsTr("Fx")
             : qsTr("Other")
    }

    implicitWidth: chipRow.implicitWidth + 16
    implicitHeight: 22
    radius: Theme.radiusS
    color: Qt.rgba(Theme.categoryColors[category].r,
                   Theme.categoryColors[category].g,
                   Theme.categoryColors[category].b, Theme.isDark ? 0.16 : 0.12)
    border.color: Theme.categoryColors[category]
    border.width: 1

    Row {
        id: chipRow
        anchors.centerIn: parent
        spacing: 5

        Rectangle {
            width: 7; height: 7; radius: 3.5
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.categoryColors[root.category]
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.label(root.category)
            color: Theme.text
            font.pixelSize: Theme.fontSizeCaption
        }
    }
}
