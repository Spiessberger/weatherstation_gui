# The QML module "Ui" (PanoramaView). The app links uiplugin and imports Ui.
qt_add_library(ui STATIC)
qt_add_qml_module(ui
    URI Ui
    # Tooling finds a module only in a directory named after its URI.
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Ui
    SOURCES
        panoramageometry.cpp panoramageometry.h
        panoramaview.cpp panoramaview.h
)
target_include_directories(ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ui PUBLIC Qt6::Quick)
