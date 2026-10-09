qt_add_library(ui STATIC panoramageometry.cpp panoramageometry.h)
target_include_directories(ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ui PUBLIC Qt6::Core)
