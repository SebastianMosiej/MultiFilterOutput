# Downloads and extracts the Qt modules needed to build and test the plugin, from Qt's
# online repository -- the same approach as Qt Creator's own plugin wizard template
# (share/qtcreator/templates/wizards/qtcreatorplugin/github_workflows_build_cmake.yml), kept in
# a script of its own so it can also be run and checked outside of GitHub Actions.
#
#   cmake -DQT_VERSION=6.11.0 -DPLATFORM=windows_x64 -DDEST=<dir> [-DDRY_RUN=ON]
#         [-DMODULES=<list>] -P download_qt.cmake
#
# PLATFORM: windows_x64 | linux_x64. DRY_RUN only resolves and prints the archive URLs.
# Modules (default all three): qtbase has everything the plugin and its tests link (Core,
# Gui, Widgets, Network, Concurrent, Xml, Sql, PrintSupport, Test); qtdeclarative has Qml,
# which Qt Creator's own libraries load at runtime; qttasktree is Qt6::TaskTree, which the
# official Qt Creator packages' Utils links (their CMake package fails to configure without
# it) -- a self-built Qt Creator bundles its own copy instead.

foreach(var QT_VERSION PLATFORM DEST)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "${var} is required")
  endif()
endforeach()

# The repository keeps one folder per Qt version, and -- on Windows, unlike on Linux -- in it
# one per compiler.
string(REPLACE "." "" qt_version_dotless "${QT_VERSION}")
if(PLATFORM STREQUAL "windows_x64")
  set(url_os "windows_x86")
  set(compiler_id "win64_msvc2022_64")
  set(repository "qt6_${qt_version_dotless}/qt6_${qt_version_dotless}_msvc2022_64")
elseif(PLATFORM STREQUAL "linux_x64")
  set(url_os "linux_x64")
  set(compiler_id "linux_gcc_64")
  set(repository "qt6_${qt_version_dotless}/qt6_${qt_version_dotless}")
else()
  message(FATAL_ERROR "Unsupported PLATFORM: ${PLATFORM}")
endif()

set(qt_base_url "https://download.qt.io/online/qtsdkrepository/${url_os}/desktop/${repository}")

file(MAKE_DIRECTORY "${DEST}")
file(DOWNLOAD "${qt_base_url}/Updates.xml" "${DEST}/Updates.xml" STATUS status)
list(GET status 0 status_code)
if(NOT status_code EQUAL 0)
  message(FATAL_ERROR "Could not download ${qt_base_url}/Updates.xml: ${status}")
endif()

# The package sections for this compiler, without the "debug info" ones. ";" is CMake's list
# separator but also occurs in the XML (e.g. "&lt;" in descriptions), which would split
# sections apart -- so it is swapped for a placeholder first; none of the fields read below
# contains one.
file(READ "${DEST}/Updates.xml" updates_xml)
string(REPLACE ";" "<semicolon>" updates_xml "${updates_xml}")
string(REGEX REPLACE "<PackageUpdate>" ";<PackageUpdate>" sections "${updates_xml}")
list(FILTER sections EXCLUDE REGEX "<Name>.*debug.*</Name>")
list(FILTER sections INCLUDE REGEX "<Name>.*${compiler_id}.*</Name>")

function(archive_url outvar module)
  set(filtered ${sections})
  list(FILTER filtered INCLUDE REGEX "<DownloadableArchives>.*${module}.*</DownloadableArchives>")
  list(LENGTH filtered count)
  if(count LESS 1)
    message(FATAL_ERROR "No ${module} archive for ${compiler_id} in ${qt_base_url}/Updates.xml")
  endif()
  list(GET filtered 0 section)
  string(REGEX MATCH "<Name>(.*)</Name>" match "${section}")
  set(name ${CMAKE_MATCH_1})
  string(REGEX MATCH "<Version>(.*)</Version>" match "${section}")
  set(version ${CMAKE_MATCH_1})
  string(REGEX MATCH "<DownloadableArchives>.*(${module}[^,]+).*</DownloadableArchives>" match "${section}")
  set(archive ${CMAKE_MATCH_1})
  set(${outvar} "${qt_base_url}/${name}/${version}${archive}" PARENT_SCOPE)
endfunction()

function(download_and_extract module subdir)
  archive_url(url ${module})
  message("${module}: ${url}")
  if(DRY_RUN)
    return()
  endif()
  set(archive "${DEST}/${module}.7z")
  file(DOWNLOAD "${url}" "${archive}" STATUS status)
  list(GET status 0 status_code)
  if(NOT status_code EQUAL 0)
    message(FATAL_ERROR "Could not download ${url}: ${status}")
  endif()
  file(MAKE_DIRECTORY "${DEST}/${subdir}")
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar xf "${archive}"
    WORKING_DIRECTORY "${DEST}/${subdir}"
    RESULT_VARIABLE result)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "Could not extract ${archive}")
  endif()
  file(REMOVE "${archive}")
endfunction()

if(NOT DEFINED MODULES)
  set(MODULES qtbase qtdeclarative qttasktree)
endif()
foreach(module IN LISTS MODULES)
  download_and_extract(${module} "")
endforeach()
# uic and the Qt libraries depend on libicu*.so on Linux.
if(PLATFORM STREQUAL "linux_x64" AND "qtbase" IN_LIST MODULES)
  download_and_extract(icu "lib")
endif()
