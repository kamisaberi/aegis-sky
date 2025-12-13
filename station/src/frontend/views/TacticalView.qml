import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import Aegis.Backend 1.0   // CoreClient, VideoReceiver
import Aegis.Tactical 1.0  // TrackStore
import "../components"     // VideoHud

Item {
    id: tacticalRoot

    // =========================================================================
    // 1. BACKEND SYSTEMS
    // =========================================================================

    // The Data Model holding active targets (Shared with HUD)
    TrackStore {
        id: liveTracks
    }

    // The Network Client (TCP/IP)
    CoreClient {
        id: comms

        // Signal Handler: Update UI text when telemetry arrives
        onTelemetryReceived: (ts, pan, tilt) => {
            telemetryLabel.text = "GIMBAL: [" + pan.toFixed(1) + "°, " + tilt.toFixed(1) + "°] | T: " + ts.toFixed(2) + "s"

            // Sync status light
            linkStatus.color = "#00FF00" // Green
            linkTimer.restart() // Keep alive logic
        }

        // Signal Handler: Update TrackStore when new targets arrive
        onTargetsUpdated: (targetList) => {
            // Note: targetList is a QVariantList of structs converted by C++
            liveTracks.updateTracks(targetList)
        }
    }

    // The Low-Latency Video Pipeline
    VideoReceiver {
        id: videoStream
        uri: "test" // Default to Test Pattern until connected
        videoSink: mainOutput.videoSink // Bind C++ GStreamer Sink to QML Output
    }

    // Watchdog Timer to turn status red if telemetry stops
    Timer {
        id: linkTimer
        interval: 1000 // 1 second timeout
        onTriggered: linkStatus.color = "#555" // Grey/Offline
    }

    // =========================================================================
    // 2. UI LAYOUT
    // =========================================================================
    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ---------------------------------------------------------------------
        // LEFT: VIDEO FEED & HUD (75% Width)
        // ---------------------------------------------------------------------
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: parent.width * 0.75
            color: "black"

            // A. The Video Surface
            VideoOutput {
                id: mainOutput
                anchors.fill: parent
                fillMode: VideoOutput.PreserveAspectCrop
            }

            // B. The Augmented Reality Overlay
            VideoHud {
                id: hudOverlay
                anchors.fill: parent
                trackModel: liveTracks // Bind to our data model

                // Camera Intrinsics (Must match Simulator/Hardware)
                fovHorizontal: 60.0
                fovVertical: 33.75
            }

            // C. Static Crosshair
            Item {
                anchors.centerIn: parent
                width: 100; height: 100

                // Horizontal line
                Rectangle {
                    width: 40; height: 2; color: "red"; opacity: 0.6
                    anchors.centerIn: parent
                }
                // Vertical line
                Rectangle {
                    width: 2; height: 40; color: "red"; opacity: 0.6
                    anchors.centerIn: parent
                }
                // Gap
                Rectangle {
                    width: 4; height: 4; color: "black"; opacity: 0.1
                    anchors.centerIn: parent
                }
            }

            // D. On-Screen Telemetry Overlay
            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: 20
                width: 400
                height: 40
                color: "#80000000" // Semi-transparent black
                radius: 4

                Row {
                    anchors.centerIn: parent
                    spacing: 15

                    // Connection Dot
                    Rectangle {
                        id: linkStatus
                        width: 12; height: 12; radius: 6
                        color: "#555" // Starts offline
                        border.color: "white"; border.width: 1
                    }

                    // Text Data
                    Text {
                        id: telemetryLabel
                        text: "WAITING FOR LINK..."
                        color: "#00FF00" // Terminal Green
                        font.family: "Roboto Mono"
                        font.bold: true
                        font.pixelSize: 16
                    }
                }
            }
        }

        // ---------------------------------------------------------------------
        // RIGHT: COMMAND PANEL (25% Width)
        // ---------------------------------------------------------------------
        Rectangle {
            Layout.fillHeight: true
            Layout.fillWidth: true
            color: "#1E1E1E" // Dark Grey
            border.left: 1; border.color: "#444"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 15

                // --- HEADER ---
                Label {
                    text: "SYSTEM CONTROL"
                    color: "white"
                    font.pixelSize: 22
                    font.bold: true
                    Layout.alignment: Qt.AlignHCenter
                }

                Rectangle { height: 1; Layout.fillWidth: true; color: "#555" }

                // --- CONNECTION ---
                GroupBox {
                    title: "Network Link"
                    Layout.fillWidth: true
                    label: Label { color: "lightgrey"; font.bold: true }

                    ColumnLayout {
                        width: parent.width
                        TextField {
                            id: ipField
                            text: "127.0.0.1"
                            Layout.fillWidth: true
                            placeholderText: "Core IP Address"
                            color: "white"
                            background: Rectangle { color: "#333"; border.color: "#555" }
                        }

                        Button {
                            text: comms.isConnected ? "DISCONNECT" : "CONNECT TO CORE"
                            Layout.fillWidth: true
                            highlighted: true

                            onClicked: {
                                if (comms.isConnected) {
                                    comms.disconnectFromCore()
                                    videoStream.uri = "test"
                                    videoStream.start()
                                } else {
                                    comms.hostAddress = ipField.text
                                    comms.connectToCore()
                                    // Switch to Live Stream
                                    videoStream.uri = "udp://" + ipField.text + ":5000"
                                    videoStream.start()
                                }
                            }
                        }
                    }
                }

                // --- GIMBAL MANUAL OVERRIDE ---
                GroupBox {
                    title: "Gimbal Override"
                    Layout.fillWidth: true
                    label: Label { color: "lightgrey"; font.bold: true }

                    GridLayout {
                        columns: 2
                        width: parent.width

                        Label { text: "PAN Vel:"; color: "white" }
                        Slider {
                            id: panSlider
                            Layout.fillWidth: true
                            from: -2.0; to: 2.0; value: 0.0
                            onMoved: comms.setGimbalVector(value, tiltSlider.value)
                            onPressedChanged: { if(!pressed) value = 0.0; } // Auto-center
                        }

                        Label { text: "TILT Vel:"; color: "white" }
                        Slider {
                            id: tiltSlider
                            Layout.fillWidth: true
                            from: -1.0; to: 1.0; value: 0.0
                            onMoved: comms.setGimbalVector(panSlider.value, value)
                            onPressedChanged: { if(!pressed) value = 0.0; } // Auto-center
                        }
                    }
                }

                Item { Layout.fillHeight: true } // Spacer

                // --- WEAPONS RELEASE ---
                Rectangle { height: 1; Layout.fillWidth: true; color: "#555" }

                Label {
                    text: "ENGAGEMENT AUTHORITY"
                    color: "red"
                    font.bold: true
                    Layout.alignment: Qt.AlignHCenter
                }

                // Master Arm Switch
                Switch {
                    id: armSwitch
                    text: checked ? "SYSTEM ARMED" : "SAFE"
                    Layout.alignment: Qt.AlignHCenter

                    contentItem: Text {
                        text: armSwitch.text
                        font.bold: true
                        color: armSwitch.checked ? "red" : "lime"
                        leftPadding: armSwitch.indicator.width + armSwitch.spacing
                        verticalAlignment: Text.AlignVCenter
                    }

                    onCheckedChanged: comms.setSystemArmed(checked)
                }

                // The Big Red Button
                Button {
                    id: fireBtn
                    text: "FIRE INTERCEPTOR"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 80
                    enabled: armSwitch.checked && comms.isConnected
                    
                    background: Rectangle {
                        color: parent.down ? "#500000" : (parent.enabled ? "#CC0000" : "#333")
                        border.color: parent.down ? "white" : "transparent"
                        border.width: 4
                        radius: 6
                    }

                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: 24
                        font.bold: true
                        color: parent.enabled ? "white" : "#555"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onPressed: comms.sendFireCommand()
                }
            }
        }
    }
}