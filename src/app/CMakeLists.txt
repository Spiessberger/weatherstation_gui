# App-level code the tests can link; main.cpp stays a thin wrapper around it.
qt_add_library(app_core STATIC config.cpp config.h logging.cpp logging.h)
target_include_directories(app_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(app_core PUBLIC Qt6::Core)

qt_add_executable(weatherstation_gui main.cpp)
qt_add_qml_module(weatherstation_gui
    URI App
    QML_FILES Main.qml
    # Lets qmlcachegen and qmllint find the types of Main.qml's imports.
    DEPENDENCIES TARGET panomax TARGET ui
)
target_compile_definitions(weatherstation_gui PRIVATE APP_VERSION="${PROJECT_VERSION}")
target_link_libraries(weatherstation_gui PRIVATE app_core Qt6::Quick panomaxplugin uiplugin)
