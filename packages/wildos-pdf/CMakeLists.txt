cmake_minimum_required(VERSION 3.25)
project(wildos-pdf VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(GNUInstallDirs)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets DBus PrintSupport Qml Quick QuickControls2)
find_package(Qt6Keychain REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(POPPLER REQUIRED IMPORTED_TARGET poppler-qt6)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(wildos-pdf src/main.cpp)
target_compile_definitions(wildos-pdf PRIVATE WILDOS_PDF_VERSION="${PROJECT_VERSION}" QT_NO_KEYWORDS)

file(GLOB QML_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} qml/*.qml)
set_source_files_properties(qml/PdfIcons.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
foreach(file ${QML_SOURCES})
    get_filename_component(name ${file} NAME)
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()

qt_add_qml_module(wildos-pdf
    URI WildOS.PDF
    QML_FILES ${QML_SOURCES}
    RESOURCES data/wildos-pdf.svg
    SOURCES
        src/document.cpp src/document.h
        src/pageprovider.cpp src/pageprovider.h
        src/signatures.cpp src/signatures.h
)

target_include_directories(wildos-pdf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(wildos-pdf PRIVATE
    Qt6::Widgets Qt6::Gui Qt6::DBus Qt6::PrintSupport Qt6::Qml Qt6::Quick Qt6::QuickControls2
    qt6keychain
    PkgConfig::POPPLER
)

install(TARGETS wildos-pdf RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/org.wildos.pdf.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES data/org.wildos.pdf.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
# Also in the WildOS icon theme (wildos-branding); here too so PDF has its
# icon under any theme.
install(FILES data/wildos-pdf.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
