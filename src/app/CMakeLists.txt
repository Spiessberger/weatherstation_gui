qt_add_executable(weatherstation_gui main.cpp)
qt_add_qml_module(weatherstation_gui
    URI App
    QML_FILES Main.qml
)
target_link_libraries(weatherstation_gui PRIVATE Qt6::Quick)
