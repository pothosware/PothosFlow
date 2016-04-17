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
    StringEntry.cpp
)

POTHOS_MODULE_UTIL(
    TARGET CoreEditWidgets
    SOURCES ${SOURCES}
    LIBRARIES ${Qt5_LIBRARIES}
    DESTINATION gui
)
