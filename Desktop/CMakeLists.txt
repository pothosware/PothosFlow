########################################################################
# menu launcher and MIME type
########################################################################
file(GLOB pothos_gui_png_icons ${CMAKE_CURRENT_SOURCE_DIR}/pothos-gui-*.png)

install(
    FILES
        pothos-gui.desktop
        pothos-gui.xml
        ${pothos_gui_png_icons}
    DESTINATION share/Pothos/Desktop
    COMPONENT pothos_gui
)

########################################################################
# install/uninstall scripts
########################################################################
install(
    PROGRAMS
        install
        uninstall
    DESTINATION share/Pothos/Desktop
    COMPONENT pothos_gui
)
