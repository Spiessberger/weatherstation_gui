# Panomax webcam library: API data and client, and the PanomaxImageSource QML type.
qt_add_library(panomax STATIC)
qt_add_qml_module(panomax
    URI Panomax
    # Tooling finds a module only in a directory named after its URI.
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Panomax
    SOURCES
        panomax.cpp panomax.h
        panomaxclient.cpp panomaxclient.h
)
target_include_directories(panomax PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(panomax PUBLIC Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml)
