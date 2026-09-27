cmake_minimum_required(VERSION 3.16)

project(MultiFilterOutput)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(QtCreator REQUIRED COMPONENTS Core ProjectExplorer TextEditor)
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
    QtCreator::TextEditor
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
    src/linequeue.h
    src/linequeue.cpp
    src/linerouter.h
    src/linerouter.cpp
    src/patternlibrary.h
    src/patternlibrary.cpp
    src/patternlibrarysettingspage.h
    src/patternlibrarysettingspage.cpp
    src/outputpanesettings.h
    src/outputpanesettings.cpp
    src/filtersetaspect.h
    src/filtersetaspect.cpp
    src/filereference.h
    src/filereference.cpp
    src/multifilterlineparser.h
    src/multifilterlineparser.cpp
    src/outputpanetabbar.h
    src/outputpanetabbar.cpp
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

# Where the Qt Creator we build against keeps its binaries. LOCATION is the imported
# Core.dll/.so/.dylib under <prefix>/lib/qtcreator/plugins, so <prefix> is three levels up.
# Used both to find the qtcreator executable for the Run button and to put Qt Creator's own
# libraries on the tests' PATH below.
get_target_property(QtCreatorCorePath QtCreator::Core LOCATION)
get_filename_component(QtCreatorPluginDir "${QtCreatorCorePath}" DIRECTORY)
get_filename_component(QtCreatorPrefix "${QtCreatorPluginDir}/../../.." ABSOLUTE)

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
  add_qtc_test(tst_linebufferfuzz
    SOURCES tests/tst_linebufferfuzz.cpp
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
  add_qtc_test(tst_outputtabwidget
    SOURCES tests/tst_outputtabwidget.cpp
    DEPENDS MultiFilterOutput
  )
  add_qtc_test(tst_linequeue
    SOURCES tests/tst_linequeue.cpp
    DEPENDS MultiFilterOutput
  )
  add_qtc_test(tst_linerouter
    SOURCES tests/tst_linerouter.cpp
    DEPENDS MultiFilterOutput
  )
  add_qtc_test(tst_runtracker
    SOURCES tests/tst_runtracker.cpp
    DEPENDS MultiFilterOutput QtCreator::ProjectExplorer
  )

  # add_qtc_test() bakes a fixed PATH into each test's ENVIRONMENT property (see
  # finalize_test_setup() in QtCreatorAPIInternal.cmake). That PATH covers this project's
  # own binary dir and Qt, but not the Qt Creator installation the tests link against --
  # for an in-tree plugin those are the same directory, for an out-of-tree one they are
  # not. Without this every test aborts at load time with STATUS_DLL_NOT_FOUND
  # (0xC0000135) before a single case runs. ENVIRONMENT_MODIFICATION is applied after
  # ENVIRONMENT, so this prepends to what add_qtc_test() already set rather than
  # replacing it.
  if(WIN32)
    set(QtCreatorRuntimeDirs "${QtCreatorPluginDir}" "${QtCreatorPrefix}/bin")
    set(QtCreatorPathModification "")
    foreach(dir IN LISTS QtCreatorRuntimeDirs)
      list(APPEND QtCreatorPathModification "PATH=path_list_prepend:${dir}")
    endforeach()
    set_tests_properties(
      tst_pattern
      tst_linematcher
      tst_linebuffer
      tst_linebufferfuzz
      tst_patternlibrary
      tst_filtersetaspect
      tst_filereference
      tst_tablabel
      tst_outputtabwidget
      tst_linequeue
      tst_linerouter
      tst_runtracker
      PROPERTIES ENVIRONMENT_MODIFICATION "${QtCreatorPathModification}"
    )
  endif()
endif()

# Enable the Run button in Qt Creator
find_program(QtCreatorExecutable
  NAMES qtcreator "Qt Creator"
  PATHS "${QtCreatorPrefix}/bin" "${QtCreatorPrefix}/MacOS"
  NO_DEFAULT_PATH
)
if(QtCreatorExecutable)
  add_custom_target(RunQtCreator
    COMMAND ${QtCreatorExecutable} -pluginpath $<TARGET_FILE_DIR:MultiFilterOutput>
    DEPENDS MultiFilterOutput
  )
  set_target_properties(RunQtCreator PROPERTIES FOLDER "qtc_runnable")
endif()
