cmake_minimum_required(VERSION 3.16)

project(MultiFilterOutput)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(QtCreator REQUIRED COMPONENTS Core ProjectExplorer)
find_package(Qt6 COMPONENTS Widgets REQUIRED)

option(WITH_TESTS "Builds with tests" NO)

if(WITH_TESTS)
  find_package(Qt6 REQUIRED COMPONENTS Test)
  set(IMPLICIT_DEPENDS Qt::Test)
  enable_testing()
endif()

add_qtc_plugin(MultiFilterOutput
  PLUGIN_DEPENDS
    QtCreator::Core
    QtCreator::ProjectExplorer
  DEPENDS
    Qt::Widgets
    QtCreator::ExtensionSystem
    QtCreator::Utils
  SOURCES
    src/multifilteroutput_global.h
    src/multifilteroutputconstants.h
    src/multifilteroutputtr.h
    src/multifilteroutputplugin.h
    src/multifilteroutputplugin.cpp
    src/pattern.h
    src/pattern.cpp
    src/linematcher.h
    src/linematcher.cpp
    src/linebuffer.h
    src/linebuffer.cpp
    src/patternlibrary.h
    src/patternlibrary.cpp
    src/patternlibrarysettingspage.h
    src/patternlibrarysettingspage.cpp
    src/filtersetaspect.h
    src/filtersetaspect.cpp
    src/filereference.h
    src/filereference.cpp
    src/multifilterlineparser.h
    src/multifilterlineparser.cpp
    src/outputtabwidget.h
    src/outputtabwidget.cpp
    src/runtracker.h
    src/runtracker.cpp
    src/tablabel.h
    src/tablabel.cpp
    src/filteredoutputpane.h
    src/filteredoutputpane.cpp
    MultiFilterOutput.qrc
)

target_include_directories(MultiFilterOutput PRIVATE src)

if(WITH_TESTS)
  add_qtc_test(tst_pattern
    SOURCES tests/tst_pattern.cpp
    DEPENDS MultiFilterOutput
  )
  add_qtc_test(tst_linematcher
    SOURCES tests/tst_linematcher.cpp
    DEPENDS MultiFilterOutput
  )
  add_qtc_test(tst_linebuffer
    SOURCES tests/tst_linebuffer.cpp
    DEPENDS MultiFilterOutput
  )
  add_qtc_test(tst_patternlibrary
    SOURCES tests/tst_patternlibrary.cpp
    DEPENDS MultiFilterOutput
  )
  add_qtc_test(tst_filtersetaspect
    SOURCES tests/tst_filtersetaspect.cpp
    DEPENDS MultiFilterOutput
  )
  add_qtc_test(tst_filereference
    SOURCES tests/tst_filereference.cpp
    DEPENDS MultiFilterOutput
  )
  add_qtc_test(tst_tablabel
    SOURCES tests/tst_tablabel.cpp
    DEPENDS MultiFilterOutput
  )
endif()

# Enable the Run button in Qt Creator
get_target_property(QtCreatorCorePath QtCreator::Core LOCATION)
find_program(QtCreatorExecutable
  NAMES qtcreator "Qt Creator"
  PATHS "${QtCreatorCorePath}/../../../bin" "${QtCreatorCorePath}/../../../MacOS"
  NO_DEFAULT_PATH
)
if(QtCreatorExecutable)
  add_custom_target(RunQtCreator
    COMMAND ${QtCreatorExecutable} -pluginpath $<TARGET_FILE_DIR:MultiFilterOutput>
    DEPENDS MultiFilterOutput
  )
  set_target_properties(RunQtCreator PROPERTIES FOLDER "qtc_runnable")
endif()
