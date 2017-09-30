########################################################################
# Project setup
########################################################################
cmake_minimum_required(VERSION 2.8.9)
project(PothosFlow CXX)

if(${CMAKE_PROJECT_NAME} STREQUAL ${PROJECT_NAME})
    find_package(Pothos "0.6.0" CONFIG REQUIRED)
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
cmake_dependent_option(ENABLE_FLOW "Enable Pothos Flow component" ON "Pothos_FOUND;Qt5Widgets_FOUND;Qt5Concurrent_FOUND" OFF)
add_feature_info(Flow ENABLE_FLOW "GUI designer tool for the Pothos framework")
if (NOT ENABLE_FLOW)
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
    LIBRARY DESTINATION lib${LIB_SUFFIX} COMPONENT pothos_flow # .so file
    ARCHIVE DESTINATION lib${LIB_SUFFIX} COMPONENT pothos_flow # .lib file
    RUNTIME DESTINATION bin              COMPONENT pothos_flow # .dll file
)
list(APPEND Pothos_LIBRARIES PothosQtColorPicker)

########################################################################
# Bundle icon resources with executable
########################################################################
set(CMAKE_AUTORCC ON)
file(GLOB ICONS
    ${CMAKE_CURRENT_SOURCE_DIR}/icons/*.png
    ${CMAKE_CURRENT_SOURCE_DIR}/icons/*.gif)
set(RESOURCES_QRC ${CMAKE_CURRENT_BINARY_DIR}/resources.qrc)
file(WRITE ${RESOURCES_QRC} "<!DOCTYPE RCC><RCC version='1.0'>\n")
file(APPEND ${RESOURCES_QRC} "<qresource>\n")
foreach(ICON ${ICONS})
    get_filename_component(NAME ${ICON} NAME)
    file(APPEND ${RESOURCES_QRC} "<file alias='icons/${NAME}'>${ICON}</file>\n")
endforeach(ICON)
file(APPEND ${RESOURCES_QRC} "</qresource>\n")
file(APPEND ${RESOURCES_QRC} "</RCC>\n")
list(APPEND SOURCES ${RESOURCES_QRC})

########################################################################
# Resource file - adds an icon to Pothos Flow executable
########################################################################
if (WIN32)
    set(ICON_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/icons/PothosFlow.ico)
    set(RES_FILES "${CMAKE_CURRENT_BINARY_DIR}/PothosFlow.rc")
    file(WRITE "${RES_FILES}" "id ICON \"${ICON_SOURCE}\"")
    set(CMAKE_RC_COMPILER_INIT windres)
    enable_language(RC)
    set(CMAKE_RC_COMPILE_OBJECT
        "<CMAKE_RC_COMPILER> <FLAGS> -O coff <DEFINES> -i <SOURCE> -o <OBJECT>")
    list(APPEND SOURCES ${RES_FILES})
endif (WIN32)

if (APPLE)
    set(ICON_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/icons/PothosFlow.icns)
    set_source_files_properties(${ICON_SOURCE}
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    list(APPEND SOURCES ${ICON_SOURCE})
endif (APPLE)

########################################################################
# sources list
########################################################################
list(APPEND SOURCES
    PothosFlow.cpp

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

    EvalEngine/EvalTracer.cpp
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

set(BUNDLE_DESTINATION bin CACHE STRING "OSX bundle destination")

#under MSVC build a win32 app so a console is not launched with the GUI exe
if (MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "/entry:mainCRTStartup ${CMAKE_EXE_LINKER_FLAGS}")
    add_executable(PothosFlow WIN32 ${SOURCES})

#under OSX build a bundle so the menu integration works properly
#this also creates a launcher with an icon when opened in finder
elseif (APPLE)
    add_executable(PothosFlow MACOSX_BUNDLE ${SOURCES})
    string(TIMESTAMP Pothos_BUNDLE_VERSION "%Y.%m.%d")
    set_target_properties(PothosFlow PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_STRING "Graphical designer for the Pothos data-flow framework"
        MACOSX_BUNDLE_BUNDLE_NAME "PothosFlow"
        MACOSX_BUNDLE_BUNDLE_VERSION "${Pothos_BUNDLE_VERSION}"
        MACOSX_BUNDLE_LONG_VERSION_STRING "${Pothos_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${Pothos_VERSION_MAJOR}.${Pothos_VERSION_MINOR}.${Pothos_VERSION_PATCH}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.pothosware.pothosflow"
        MACOSX_BUNDLE_ICON_FILE "PothosFlow.icns"
        MACOSX_BUNDLE_COPYRIGHT "Copyright (c) 2013-2017 Josh Blum"
    )
    #provide PothosFlow command line script which calls into the bundle
    set(ABSOLUTE_BUNDLE_DESTINATION ${BUNDLE_DESTINATION})
    if (NOT IS_ABSOLUTE "${BUNDLE_DESTINATION}")
        set(ABSOLUTE_BUNDLE_DESTINATION ${CMAKE_INSTALL_PREFIX}/${BUNDLE_DESTINATION})
    endif ()
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/PothosFlowOSX.sh
        ${CMAKE_CURRENT_BINARY_DIR}/PothosFlow @ONLY)
    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/PothosFlow DESTINATION bin)

#otherwise build a standard UNIX style executable
else ()
    add_executable(PothosFlow ${SOURCES})
    add_subdirectory(Desktop) #freedesktop.org
endif ()

target_link_libraries(PothosFlow ${Pothos_LIBRARIES})

install(
    TARGETS PothosFlow
    BUNDLE DESTINATION ${BUNDLE_DESTINATION}
    RUNTIME DESTINATION bin
    COMPONENT pothos_flow
)

########################################################################
# Edit widgets module
########################################################################
add_subdirectory(EditWidgets)
