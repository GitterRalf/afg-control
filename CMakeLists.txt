cmake_minimum_required(VERSION 3.21)
project(AFG-Control LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets LinguistTools)
qt_standard_project_setup()
set(QT_I18N_SOURCE_LANGUAGE de)
set(CMAKE_AUTORCC ON)

qt_add_executable(AFG-Control WIN32
    main.cpp mainwindow.cpp mainwindow.h
    afg.cpp afg.h gpib.cpp gpib.h version.h
    assets/app.qrc assets/app.rc
)
target_link_libraries(AFG-Control PRIVATE Qt6::Widgets)
target_include_directories(AFG-Control PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/assets")
target_compile_definitions(AFG-Control PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(AFG-Control PRIVATE /utf-8)
endif()
qt_add_translations(AFG-Control TS_FILES translations/afg_en.ts translations/afg_fr.ts)
foreach(locale IN ITEMS de fr)
    set(qt_catalog "${Qt6_DIR}/../../../translations/qtbase_${locale}.qm")
    if(EXISTS "${qt_catalog}")
        get_filename_component(qt_catalog "${qt_catalog}" ABSOLUTE)
        set_source_files_properties("${qt_catalog}" PROPERTIES QT_RESOURCE_ALIAS "qtbase_${locale}.qm")
        qt_add_resources(AFG-Control "qt_i18n_${locale}" PREFIX "/qt-i18n" FILES "${qt_catalog}")
    endif()
endforeach()
