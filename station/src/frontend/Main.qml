import QtQuick
import QtQuick.Controls
import QtQuick.Window
import "views" // Import our custom views folder

ApplicationWindow {
    id: rootWindow
    visible: true
    width: 1920
    height: 1080
    title: "AEGIS SKY // STATION COMMAND [UNCLASSIFIED]"
    color: "#121212" // Deep Dark Grey (Military Theme)

    // Global Font Setup
    font.family: "Roboto Mono"
    font.pixelSize: 14

    // Top Navigation Bar
    header: ToolBar {
        background: Rectangle { color: "#1E1E1E"; border.bottom: 1; border.color: "#333" }
        Row {
            padding: 10
            spacing: 20
            Label { 
                text: "AURA ENGINE v1.0" 
                color: "#00FF00" // Hacker Green
                font.bold: true 
                anchors.verticalCenter: parent.verticalCenter
            }
            
            TabBar {
                id: navBar
                background: Rectangle { color: "transparent" }
                TabButton { text: "TACTICAL"; width: 120 }
                TabButton { text: "SYSTEM HEALTH"; width: 120 }
                TabButton { text: "CONFIG"; width: 120 }
            }
        }
    }

    // View Switcher
    StackLayout {
        anchors.fill: parent
        currentIndex: navBar.currentIndex

        TacticalView { id: tacticalView }   // Index 0
        Rectangle { color: "#222" }         // Index 1 (Placeholder)
        Rectangle { color: "#333" }         // Index 2 (Placeholder)
    }
}