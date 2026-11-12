# The Remote Desktop engine, shared by Windows mode's window client
# (wildos-rdp) and the Remote app (wildos-remote): the FreeRDP connection
# and its graphics pipeline, clipboard, sound and drives, reconnecting,
# keyboard layout and input, and the whole-desktop session.
#
# include() it from either package; it adds the static library
# wildos-rdp-engine, with its headers on the include path.

set(WILDOS_RDP_ENGINE_DIR ${CMAKE_CURRENT_LIST_DIR})

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Network)
find_package(PkgConfig REQUIRED)
# 3.32 is the first FreeRDP that drew RemoteApp windows properly on Wild OS
# hardware; 3.15 drew only the first frame of each window.
pkg_check_modules(FREERDP REQUIRED IMPORTED_TARGET freerdp3>=3.32 freerdp-client3>=3.32 winpr3>=3.32)

add_library(wildos-rdp-engine STATIC
    ${WILDOS_RDP_ENGINE_DIR}/src/client.cpp ${WILDOS_RDP_ENGINE_DIR}/src/client.h
    ${WILDOS_RDP_ENGINE_DIR}/src/clipboard.cpp ${WILDOS_RDP_ENGINE_DIR}/src/clipboard.h
    ${WILDOS_RDP_ENGINE_DIR}/src/keymap.cpp ${WILDOS_RDP_ENGINE_DIR}/src/keymap.h
    ${WILDOS_RDP_ENGINE_DIR}/src/input.cpp ${WILDOS_RDP_ENGINE_DIR}/src/input.h
    ${WILDOS_RDP_ENGINE_DIR}/src/desktop.cpp ${WILDOS_RDP_ENGINE_DIR}/src/desktop.h
    ${WILDOS_RDP_ENGINE_DIR}/src/monitors.cpp ${WILDOS_RDP_ENGINE_DIR}/src/monitors.h
)
set_target_properties(wildos-rdp-engine PROPERTIES AUTOMOC ON POSITION_INDEPENDENT_CODE ON)
target_include_directories(wildos-rdp-engine PUBLIC ${WILDOS_RDP_ENGINE_DIR}/src)
# Nothing here may lean on API FreeRDP has already marked for removal.
target_compile_definitions(wildos-rdp-engine PUBLIC WITHOUT_FREERDP_3x_DEPRECATED)
target_link_libraries(wildos-rdp-engine PUBLIC Qt6::Gui Qt6::Network PkgConfig::FREERDP)
