import QtQuick

Window {
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
}
