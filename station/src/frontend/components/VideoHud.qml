import QtQuick
import QtQuick.Shapes
import Aegis.Tactical 1.0

Item {
    id: hudRoot
    anchors.fill: parent
    
    // Camera Intrinsics (Must match the Simulator/Real Camera)
    property real fovHorizontal: 60.0 
    property real fovVertical: 33.75 // 16:9 aspect ratio of 60 deg

    // The Data Model
    property TrackStore trackModel: null

    Repeater {
        model: trackModel
        delegate: Item {
            id: trackDelegate
            
            // CONVERT ANGLES TO PIXELS
            // Azimuth 0 = Center of Screen
            // range [-30, 30] maps to [0, width]
            
            // Helper function to map angle to pixel
            function angleToX(angleRad) {
                var angleDeg = angleRad * (180.0 / Math.PI);
                var norm = (angleDeg + (fovHorizontal / 2.0)) / fovHorizontal;
                return norm * hudRoot.width;
            }

            function angleToY(angleRad) {
                var angleDeg = angleRad * (180.0 / Math.PI);
                // Invert Y because +Elevation is UP, but screen Y is DOWN
                var norm = 1.0 - ((angleDeg + (fovVertical / 2.0)) / fovVertical);
                return norm * hudRoot.height;
            }

            x: angleToX(model.azimuth) - (box.width / 2)
            y: angleToY(model.elevation) - (box.height / 2)

            // The Bounding Box
            Rectangle {
                id: box
                width: 60
                height: 60
                color: "transparent"
                border.color: model.isThreat ? "red" : "lime"
                border.width: 2
                
                // Pulsing Animation for Threats
                SequentialAnimation on opacity {
                    running: model.isThreat
                    loops: Animation.Infinite
                    NumberAnimation { from: 1.0; to: 0.2; duration: 500 }
                    NumberAnimation { from: 0.2; to: 1.0; duration: 500 }
                }
            }

            // Target Label
            Rectangle {
                anchors.top: box.bottom
                anchors.horizontalCenter: box.horizontalCenter
                anchors.topMargin: 4
                width: labelText.width + 10
                height: labelText.height + 4
                color: model.isThreat ? "red" : "black"
                opacity: 0.8

                Text {
                    id: labelText
                    anchors.centerIn: parent
                    text: model.label + " [" + model.trackId + "]"
                    color: "white"
                    font.pixelSize: 10
                    font.bold: true
                }
            }
            
            // Velocity Vector Line (Predictive Lead)
            Shape {
                visible: model.isThreat
                ShapePath {
                    strokeWidth: 2
                    strokeColor: "red"
                    dashPattern: [4, 2]
                    startX: box.width / 2
                    startY: box.height / 2
                    // Draw a line representing where it will be in 1 sec
                    // (Hardcoded visualization for now)
                    PathLine { x: 100; y: -50 } 
                }
            }
        }
    }
}