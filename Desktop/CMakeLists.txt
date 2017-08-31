########################################################################
# menu launcher and MIME type
########################################################################
file(GLOB pothos_flow_png_icons ${CMAKE_CURRENT_SOURCE_DIR}/pothos-flow-*.png)

install(
    FILES
        pothos-flow.desktop
        pothos-flow.xml
        ${pothos_flow_png_icons}
    DESTINATION share/Pothos/Desktop
    COMPONENT pothos_flow
)

########################################################################
# install/uninstall scripts
########################################################################
install(
    PROGRAMS
        install
        uninstall
    DESTINATION share/Pothos/Desktop
    COMPONENT pothos_flow
)
