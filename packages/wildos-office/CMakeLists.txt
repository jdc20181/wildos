cmake_minimum_required(VERSION 3.25)
project(wildos-office VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(GNUInstallDirs)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets DBus Qml Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.8)

# The start screen: wildos-office, run as wildos-write or wildos-tables
# (src/main.cpp), which hands any file straight to LibreOffice.
set(OFFICE_DATA ${CMAKE_INSTALL_FULL_DATADIR}/wildos-office)
qt_add_executable(wildos-office src/main.cpp)
target_compile_definitions(wildos-office PRIVATE
    WILDOS_OFFICE_VERSION="${PROJECT_VERSION}"
    WILDOS_OFFICE_DATA="${OFFICE_DATA}"
    QT_NO_KEYWORDS)

file(GLOB QML_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} qml/*.qml)
foreach(file ${QML_SOURCES})
    get_filename_component(name ${file} NAME)
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()

qt_add_qml_module(wildos-office
    URI WildOS.Office
    QML_FILES ${QML_SOURCES}
    RESOURCES data/org.wildos.write.svg data/org.wildos.tables.svg
    SOURCES src/office.cpp src/office.h
)
target_include_directories(wildos-office PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(wildos-office PRIVATE Qt6::Widgets Qt6::Gui Qt6::DBus Qt6::Qml Qt6::Quick Qt6::QuickControls2)

install(TARGETS wildos-office RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
foreach(link wildos-write wildos-tables)
    install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink wildos-office \$ENV{DESTDIR}${CMAKE_INSTALL_FULL_BINDIR}/${link})")
endforeach()
install(FILES data/org.wildos.write.svg data/org.wildos.tables.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

# Templates with their card pictures, and the blank document each app
# starts from (templates/make-templates.py makes them all).
install(DIRECTORY templates/write templates/tables templates/default
    DESTINATION ${OFFICE_DATA}/templates
    FILES_MATCHING PATTERN "*.ott" PATTERN "*.ots" PATTERN "*.png")

set(LO ${CMAKE_INSTALL_PREFIX}/lib/libreoffice)

# The bundled configuration layer: LibreOffice reads every .xcd in
# share/registry, ordered by each file's dependencies.
# wildos-labels.xcd gives the commands the ribbons' short UK names; tools/
# gen-ribbons.py --labels regenerates it from LibreOffice's own registry.
install(FILES lo/registry/wildos.xcd lo/registry/wildos-labels.xcd DESTINATION ${LO}/share/registry)
# The Wild OS ribbons sit beside LibreOffice's own under new names, so
# LibreOffice updates never overwrite them.
install(FILES lo/ui/swriter/notebookbar_wildos.ui DESTINATION ${LO}/share/config/soffice.cfg/modules/swriter/ui)
install(FILES lo/ui/scalc/notebookbar_wildos.ui DESTINATION ${LO}/share/config/soffice.cfg/modules/scalc/ui)
# The File button's menu, which the ribbon names in its wildos-file-menu
# property.
install(FILES lo/popupmenu/swriter/wildosfile.xml DESTINATION ${LO}/share/config/soffice.cfg/modules/swriter/popupmenu)
install(FILES lo/popupmenu/scalc/wildosfile.xml DESTINATION ${LO}/share/config/soffice.cfg/modules/scalc/popupmenu)
# The Print dialog laid out as on the WritePrint board. LibreOffice reads it
# only from this path, which libreoffice-uiconfig-common owns, so the
# package diverts LibreOffice's copy (debian/preinst, debian/postrm).
install(FILES lo/ui/vcl/printdialog.ui DESTINATION ${LO}/share/config/soffice.cfg/vcl/ui)

# The Wild OS icon themes, built from the line icons in lo/icons. LibreOffice
# finds icon themes as images_*.zip in share/config; wildos.xcd selects one.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
file(GLOB ICON_SOURCES CONFIGURE_DEPENDS lo/icons/svg/*.svg lo/icons/lucide/*.svg)
set(ICON_THEMES
    ${CMAKE_CURRENT_BINARY_DIR}/icons/images_wildos_svg.zip
    ${CMAKE_CURRENT_BINARY_DIR}/icons/images_wildos_dark_svg.zip)
add_custom_command(OUTPUT ${ICON_THEMES}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen-icons.py ${CMAKE_CURRENT_BINARY_DIR}/icons
    DEPENDS tools/gen-icons.py lo/icons/icons.txt ${ICON_SOURCES}
    VERBATIM)
add_custom_target(icon-themes ALL DEPENDS ${ICON_THEMES})
install(FILES ${ICON_THEMES} DESTINATION ${LO}/share/config)

# Unpacked bundled extensions in share/extensions are registered at start.
# This one carries the Basic library that sets the window titles.
install(DIRECTORY lo/extension/wildos-office DESTINATION ${LO}/share/extensions)

# Launchers under LibreOffice's own ids, so its windows, recent files and
# file types all find Write and Tables. wildos-app-names copies them into
# /usr/share/wildos/applications, ahead of the stock ones.
install(FILES launchers/libreoffice-writer.desktop launchers/libreoffice-calc.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/wildos/launchers)
install(FILES environment.d/70-wildos-office.conf DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/environment.d)
