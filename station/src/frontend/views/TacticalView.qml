import QtQuick
import QtQuick.Controls
import QtMultimedia
import Aegis.Backend 1.0 // Import C++ Classes

Item {
    id: tacticalRoot

    // 1. Backend: Network Client
    CoreClient {
        id: comms
        hostAddress: "127.0.0.1"
        onTelemetryReceived: (ts, pan, tilt) => {
            // Update HUD text
            telemetryOverlay.text = "PAN: " + pan.toFixed(2) + "° | TILT: " + tilt.toFixed(2) + "°"
        }
    }

    // 2. Backend: Video Stream
    VideoReceiver {
        id: videoStream
        uri: "test" // Start with test pattern, switch to UDP later
        videoSink: output.videoSink // Bind C++ Sink to QML Output
    }

    // 3. UI Layer
    Row {
        anchors.fill: parent

        // LEFT: Video Feed (70% width)
        Rectangle {
            width: parent.width * 0.7
            height: parent.height
            color: "black"

            VideoOutput {
                id: output
                anchors.fill: parent
                fillMode: VideoOutput.PreserveAspectCrop
            }

            // HUD Overlay (Green text on top of video)
            Text {
                id: telemetryOverlay
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: 20
                color: "#00FF00"
                font.pixelSize: 20
                font.bold: true
                text: "NO LINK"
            }

            // Crosshair
            Rectangle {
                anchors.centerIn: parent
                width: 2; height: 40; color: "red"
            }
            Rectangle {
                anchors.centerIn: parent
                width: 40; height: 2; color: "red"
            }
        }

        // RIGHT: Controls (30% width)
        Rectangle {
            width: parent.width * 0.3
            height: parent.height
            color: "#1E1E1E"
            border.left: 1; border.color: "#444"

            Column {
                spacing: 20
                padding: 20
                width: parent.width

                Label { text: "CONNECTION"; color: "white"; font.bold: true }
                
                Row {
                    spacing: 10
                    TextField { id: ipInput; text: "127.0.0.1"; width: 150 }
                    Button {
                        text: "LINK"
                        onClicked: {
                            comms.hostAddress = ipInput.text
                            comms.connectToCore()
                            // Switch video to live UDP stream
                            videoStream.uri = "udp://127.0.0.1:5000"
                            videoStream.start()
                        }
                    }
                }

                Label { text: "FIRE CONTROL"; color: "white"; font.bold: true }
                
                Button {
                    text: "ARM SYSTEM"
                    palette.button: "red" // Qt6 Palette API
                    onClicked: comms.setSystemArmed(true)
                }

                Button {
                    text: "FIRE INTERCEPTOR"
                    width: parent.width
                    height: 80
                    font.pixelSize: 24
                    font.bold: true
                    
                    background: Rectangle {
                        color: parent.down ? "#800000" : "#FF0000"
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    
                    onPressed: comms.sendFireCommand()
                }
            }
        }
    }
}