########################################################################
# Project setup
########################################################################
cmake_minimum_required(VERSION 2.8.9)
project(PothosGui CXX)

if(${CMAKE_PROJECT_NAME} STREQUAL ${PROJECT_NAME})
    find_package(Pothos "0.4.0" CONFIG REQUIRED)
else()
    find_package(Pothos CONFIG REQUIRED) #in-tree build
endif()

########################################################################
# Local includes
########################################################################
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${Pothos_INCLUDE_DIRS})

########################################################################
# QT4 devel setup
########################################################################
#find_package(Qt4 COMPONENTS QtCore QtGui)
#if(NOT QT4_FOUND)
#    return()
#endif()
#include(${QT_USE_FILE})
#list(APPEND Pothos_LIBRARIES ${QT_LIBRARIES})

########################################################################
# QT5 devel setup
########################################################################
#http://www.kdab.com/using-cmake-with-qt-5/
# Tell CMake to run moc when necessary:
set(CMAKE_AUTOMOC ON)
# As moc files are generated in the binary dir, tell CMake
# to always look for includes there:
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Widgets finds its own dependencies.
find_package(Qt5Widgets)

if(Qt5Widgets_FOUND)
    include_directories(${Qt5Widgets_INCLUDE_DIRS})
    add_definitions(${Qt5Widgets_DEFINITIONS})
    list(APPEND Pothos_LIBRARIES ${Qt5Widgets_LIBRARIES})
endif()

# Widgets finds its own dependencies.
find_package(Qt5Concurrent)

if(Qt5Concurrent_FOUND)
    include_directories(${Qt5Concurrent_INCLUDE_DIRS})
    add_definitions(${Qt5Concurrent_DEFINITIONS})
    list(APPEND Pothos_LIBRARIES ${Qt5Concurrent_LIBRARIES})
endif()

########################################################################
## Feature registration
########################################################################
include(FeatureSummary)
include(CMakeDependentOption)
cmake_dependent_option(ENABLE_GUI "Enable Pothos GUI component" ON "Pothos_FOUND;Qt5Widgets_FOUND;Qt5Concurrent_FOUND" OFF)
add_feature_info(GUI ENABLE_GUI "GUI designer tool for the Pothos framework")
if (NOT ENABLE_GUI)
    return()
endif()

########################################################################
# Color picker library - LGPL separate library
########################################################################
include_directories(qtcolorpicker/src)
add_library(PothosQtColorPicker SHARED qtcolorpicker/src/qtcolorpicker.cpp)
set_target_properties(PothosQtColorPicker PROPERTIES DEFINE_SYMBOL "QT_QTCOLORPICKER_EXPORT")
set_target_properties(PothosQtColorPicker PROPERTIES VERSION 0)
target_link_libraries(PothosQtColorPicker ${Pothos_LIBRARIES})
install(TARGETS PothosQtColorPicker
    LIBRARY DESTINATION lib${LIB_SUFFIX} COMPONENT pothos_gui # .so file
    ARCHIVE DESTINATION lib${LIB_SUFFIX} COMPONENT pothos_gui # .lib file
    RUNTIME DESTINATION bin              COMPONENT pothos_gui # .dll file
)
list(APPEND Pothos_LIBRARIES PothosQtColorPicker)

########################################################################
# Resource file - adds an icon to Pothos GUI executable
########################################################################
if (WIN32)
    set(ICON_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/icons/PothosGui.ico)
    set(RES_FILES "${CMAKE_CURRENT_BINARY_DIR}/PothosGui.rc")
    file(WRITE "${RES_FILES}" "id ICON \"${ICON_SOURCE}\"")
    set(CMAKE_RC_COMPILER_INIT windres)
    enable_language(RC)
    set(CMAKE_RC_COMPILE_OBJECT
        "<CMAKE_RC_COMPILER> <FLAGS> -O coff <DEFINES> -i <SOURCE> -o <OBJECT>")
    list(APPEND SOURCES ${RES_FILES})
endif (WIN32)

if (APPLE)
    set(ICON_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/icons/PothosGui.icns)
    set_source_files_properties(${ICON_SOURCE}
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    list(APPEND SOURCES ${ICON_SOURCE})
endif (APPLE)

########################################################################
# sources list
########################################################################
list(APPEND SOURCES
    PothosGui.cpp

    MainWindow/MainWindow.cpp
    MainWindow/MainActions.cpp
    MainWindow/MainMenu.cpp
    MainWindow/MainToolBar.cpp
    MainWindow/MainSettings.cpp
    MainWindow/MainSplash.cpp
    MainWindow/IconUtils.cpp

    ColorUtils/ColorUtils.cpp
    ColorUtils/ColorsDialog.cpp

    PropertiesPanel/PropertiesPanelDock.cpp
    PropertiesPanel/ConnectionPropertiesPanel.cpp
    PropertiesPanel/BlockPropertiesPanel.cpp
    PropertiesPanel/BreakerPropertiesPanel.cpp
    PropertiesPanel/GraphPropertiesPanel.cpp
    PropertiesPanel/PropertyEditWidget.cpp

    MessageWindow/MessageWindowDock.cpp
    MessageWindow/LoggerDisplay.cpp
    MessageWindow/LoggerChannel.cpp

    BlockTree/BlockTreeDock.cpp
    BlockTree/BlockTreeWidget.cpp
    BlockTree/BlockTreeWidgetItem.cpp
    BlockTree/BlockCache.cpp

    AffinitySupport/AffinityZoneEditor.cpp
    AffinitySupport/AffinityZonesMenu.cpp
    AffinitySupport/AffinityZonesComboBox.cpp
    AffinitySupport/AffinityZonesDock.cpp
    AffinitySupport/CpuSelectionWidget.cpp

    HostExplorer/PluginModuleTree.cpp
    HostExplorer/PluginRegistryTree.cpp
    HostExplorer/SystemInfoTree.cpp
    HostExplorer/HostSelectionTable.cpp
    HostExplorer/HostExplorerDock.cpp

    GraphEditor/GraphState.cpp
    GraphEditor/GraphEditorTabs.cpp
    GraphEditor/GraphEditor.cpp
    GraphEditor/GraphEditorExport.cpp
    GraphEditor/GraphEditorSerialization.cpp
    GraphEditor/GraphEditorDeserialization.cpp
    GraphEditor/GraphEditorRenderedDialog.cpp
    GraphEditor/GraphEditorTopologyStats.cpp
    GraphEditor/GraphDraw.cpp
    GraphEditor/GraphDrawSelection.cpp
    GraphEditor/GraphActionsDock.cpp

    GraphObjects/GraphEndpoint.cpp
    GraphObjects/GraphObject.cpp
    GraphObjects/GraphBlock.cpp
    GraphObjects/GraphBlockUpdate.cpp
    GraphObjects/GraphBreaker.cpp
    GraphObjects/GraphConnection.cpp
    GraphObjects/GraphWidget.cpp
    GraphObjects/GraphWidgetContainer.cpp

    EvalEngine/EvalEngine.cpp
    EvalEngine/EvalEngineImpl.cpp
    EvalEngine/BlockEval.cpp
    EvalEngine/ThreadPoolEval.cpp
    EvalEngine/EnvironmentEval.cpp
    EvalEngine/TopologyEval.cpp
    EvalEngine/TopologyTraversal.cpp
)

########################################################################
# build executable
########################################################################

#under MSVC build a win32 app so a console is not launched with the GUI exe
if (MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "/entry:mainCRTStartup ${CMAKE_EXE_LINKER_FLAGS}")
    add_executable(PothosGui WIN32 ${SOURCES})

#under OSX build a bundle so the menu integration works properly
#this also creates a launcher with an icon when opened in finder
elseif (APPLE)
    add_executable(PothosGui MACOSX_BUNDLE ${SOURCES})
    string(TIMESTAMP Pothos_BUNDLE_VERSION "%Y.%m.%d")
    set_target_properties(PothosGui PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_STRING "Graphical designer for the Pothos data-flow framework"
        MACOSX_BUNDLE_BUNDLE_NAME "PothosGui"
        MACOSX_BUNDLE_BUNDLE_VERSION "${Pothos_BUNDLE_VERSION}"
        MACOSX_BUNDLE_LONG_VERSION_STRING "${Pothos_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${Pothos_VERSION_MAJOR}.${Pothos_VERSION_MINOR}.${Pothos_VERSION_PATCH}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.pothosware.pothosgui"
        MACOSX_BUNDLE_ICON_FILE "PothosGui.icns"
        MACOSX_BUNDLE_COPYRIGHT "Copyright (c) 2013-2016 Josh Blum"
    )
    #provide PothosGui command line script which calls into the bundle
    install(
        PROGRAMS PothosGuiOSX.sh
        RENAME PothosGui
        DESTINATION bin)

#otherwise build a standard UNIX style executable
else ()
    add_executable(PothosGui ${SOURCES})
endif ()

target_link_libraries(PothosGui ${Pothos_LIBRARIES})

install(
    TARGETS PothosGui
    BUNDLE DESTINATION bin
    RUNTIME DESTINATION bin
    COMPONENT pothos_gui
)

install(
    DIRECTORY icons
    DESTINATION share/Pothos
    COMPONENT pothos_gui
)

add_subdirectory(Desktop)

########################################################################
# Edit widgets module
########################################################################
add_subdirectory(EditWidgets)
