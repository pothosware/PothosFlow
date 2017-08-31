########################################################################
# Edit widgets module
########################################################################
set(SOURCES
    ComboBox.cpp
    DTypeChooser.cpp
    DoubleSpinBox.cpp
    FileEntry.cpp
    LineEdit.cpp
    SpinBox.cpp
    CheckBox.cpp
    ToggleButton.cpp
    ToggleSwitch.cpp
    StringEntry.cpp
    ColorPicker.cpp
)

POTHOS_MODULE_UTIL(
    TARGET CoreEditWidgets
    SOURCES ${SOURCES}
    LIBRARIES
        ${Qt5_LIBRARIES}
        PothosQtColorPicker
    DESTINATION flow
)
