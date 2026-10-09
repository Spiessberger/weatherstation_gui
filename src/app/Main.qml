import QtQuick
import Panomax
import Ui

Window {
    id: root

    // Set by main.cpp from the resolved config.
    required property int cameraId               // Panomax camera id
    required property string sizeVariant         // Panomax size variant name, or "auto"
    required property real autoscrollSpeed       // degrees per second
    required property string autoscrollDirection // "left" or "right"
    required property int idleTimeout            // ms without input before autoscroll resumes
    required property int pollInterval           // ms between checks for a newer image
    required property bool fullscreen            // false: a 1280x800 window

    width: 1280
    height: 800
    visibility: fullscreen ? Window.FullScreen : Window.Windowed
    color: "black"

    LoggingCategory {
        id: lcApp
        name: "ws.app"
    }

    PanomaxImageSource {
        id: source
        cameraId: root.cameraId
        size: root.sizeVariant
        pollInterval: root.pollInterval
    }

    PanoramaView {
        anchors.fill: parent
        image: source.image
        autoscrollSpeed: root.autoscrollSpeed
        autoscrollDirection: root.autoscrollDirection === "left" ? PanoramaView.Left : PanoramaView.Right
        idleTimeout: root.idleTimeout
    }

    Text {
        anchors.centerIn: parent
        visible: !source.ready
        text: qsTr("Loading panorama…")
        color: "#bbbbbb"
        font.pixelSize: 32
        // Hidden for good by the first image: the source keeps its last good image on errors.
        onVisibleChanged: console.info(lcApp, "first image shown, autoscroll running", root.autoscrollDirection,
                                       "at", root.autoscrollSpeed, "deg/s")
    }
}
