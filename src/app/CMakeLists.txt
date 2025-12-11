set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(resource_files
    resources.qrc
    dark_scheme/dark_scheme.qrc
    light_scheme/light_scheme.qrc)
list_items_prepend(resource_files "${SCANTAILOR_RESOURCES_DIR}/")

set(gui_only_sources
    RelinkablePathVisualization.cpp RelinkablePathVisualization.h
    RelinkingModel.cpp RelinkingModel.h
    RelinkingSortingModel.cpp RelinkingSortingModel.h
    RelinkingListView.cpp RelinkingListView.h
    RelinkingDialog.cpp RelinkingDialog.h
    SettingsDialog.cpp SettingsDialog.h
    FixDpiDialog.cpp FixDpiDialog.h
    LoadFilesStatusDialog.cpp LoadFilesStatusDialog.h
    ProjectCreationContext.cpp ProjectCreationContext.h
    ProjectOpeningContext.cpp ProjectOpeningContext.h
    OutOfMemoryDialog.cpp OutOfMemoryDialog.h
    ThumbnailSequence.cpp ThumbnailSequence.h
    ProjectFilesDialog.cpp ProjectFilesDialog.h
    NewOpenProjectPanel.cpp NewOpenProjectPanel.h
    SystemLoadWidget.cpp SystemLoadWidget.h
    MainWindow.cpp MainWindow.h
    main.cpp
    StatusBarPanel.cpp StatusBarPanel.h
    DefaultParamsDialog.cpp DefaultParamsDialog.h
    BatchProcessingSummaryDialog.cpp BatchProcessingSummaryDialog.h)

set(gui_only_ui_files
    AboutDialog.ui
    BatchProcessingLowerPanel.ui
    DefaultParamsDialog.ui
    FixDpiDialog.ui
    LoadFilesStatusDialog.ui
    MainWindow.ui
    NewOpenProjectPanel.ui
    OutOfMemoryDialog.ui
    ProjectFilesDialog.ui
    RelinkingDialog.ui
    RemovePagesDialog.ui
    SettingsDialog.ui
    StatusBarPanel.ui
    SystemLoadWidget.ui)

set(win32_resource_file "")
if (WIN32)
  string(REPLACE "/" "\\\\" SCANTAILOR_WIN_DIR_NATIVE "${SCANTAILOR_RESOURCES_DIR}/win32")
  configure_file("${SCANTAILOR_RESOURCES_DIR}/win32/resources.rc.in" "${CMAKE_BINARY_DIR}/resources.rc" @ONLY)
  set(rc_file "${CMAKE_BINARY_DIR}/resources.rc")
  file(GLOB win32_resources "${SCANTAILOR_RESOURCES_DIR}/win32/*.ico")
  set_source_files_properties(
      "${rc_file}" PROPERTIES
      OBJECT_DEPENDS ${win32_resources})
  if (MINGW)
    # CMake doesn't know how to process .rc files with MinGW.
    set(win32_resource_file "${CMAKE_BINARY_DIR}/win32_resources.o")
    add_custom_command(
        OUTPUT "${win32_resource_file}"
        WORKING_DIRECTORY "${SCANTAILOR_RESOURCES_DIR}/win32"
        COMMAND windres -i "${rc_file}" -o "${win32_resource_file}"
        MAIN_DEPENDENCY "${rc_file}"
        DEPENDS ${win32_resources})
  else()
    set(win32_resource_file "${rc_file}")
  endif()
endif()

# Determine if we're building a macOS bundle
set(MACOS_BUNDLE_FLAG "")
if(APPLE)
  set(MACOS_BUNDLE_FLAG MACOSX_BUNDLE)
endif()

add_executable(
    scantailor WIN32 ${MACOS_BUNDLE_FLAG} ${gui_only_sources} ${gui_only_ui_files}
    ${resource_files} ${win32_resource_file})
target_link_libraries(
    scantailor
    PRIVATE core ${EXTRA_LIBS})
target_include_directories(
    scantailor
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
    "$<TARGET_PROPERTY:TIFF::TIFF,INTERFACE_INCLUDE_DIRECTORIES>")

# macOS bundle configuration
if(APPLE)
  set_target_properties(scantailor PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "ScanTailor Spectre"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.scantailor.spectre"
    MACOSX_BUNDLE_BUNDLE_VERSION "${VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${VERSION}"
    MACOSX_BUNDLE_ICON_FILE "scantailor.icns"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/packaging/macos/Info.plist.in"
    OUTPUT_NAME "ScanTailor Spectre"
  )

  # Copy icon file into bundle Resources
  set(MACOS_ICON_FILE "${CMAKE_SOURCE_DIR}/packaging/macos/scantailor.icns")
  if(EXISTS "${MACOS_ICON_FILE}")
    set_source_files_properties("${MACOS_ICON_FILE}" PROPERTIES
      MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(scantailor PRIVATE "${MACOS_ICON_FILE}")
  endif()

  # Copy Metal shader library into bundle Resources
  if(DEFINED SCANTAILOR_METALLIB)
    add_custom_command(TARGET scantailor POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SCANTAILOR_METALLIB}"
        "$<TARGET_BUNDLE_DIR:scantailor>/Contents/Resources/default.metallib"
      COMMENT "Copying Metal shader library to app bundle..."
    )
  endif()

  # Find macdeployqt
  if(Qt6_FOUND)
    get_target_property(QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
  else()
    get_target_property(QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
  endif()
  get_filename_component(QT_BIN_DIR "${QMAKE_EXECUTABLE}" DIRECTORY)
  find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${QT_BIN_DIR}")

  if(MACDEPLOYQT_EXECUTABLE)
    # Run macdeployqt to bundle Qt frameworks
    add_custom_command(TARGET scantailor POST_BUILD
      COMMAND "${MACDEPLOYQT_EXECUTABLE}"
        "$<TARGET_BUNDLE_DIR:scantailor>"
        -always-overwrite
      COMMENT "Running macdeployqt to bundle Qt frameworks..."
    )

    # Fix missing transitive dependencies (brotli libraries)
    # macdeployqt sometimes misses these
    add_custom_command(TARGET scantailor POST_BUILD
      COMMAND /bin/bash "${CMAKE_SOURCE_DIR}/packaging/macos/fix-bundle-libs.sh" "$<TARGET_BUNDLE_DIR:scantailor>"
      COMMENT "Fixing missing library dependencies..."
    )

    # Ad-hoc code signing for local development
    add_custom_command(TARGET scantailor POST_BUILD
      COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:scantailor>"
      COMMENT "Ad-hoc signing the app bundle..."
    )
  else()
    message(WARNING "macdeployqt not found. The app bundle will not be self-contained.")
  endif()

  # Install the bundle
  install(TARGETS scantailor BUNDLE DESTINATION ".")
else()
  install(TARGETS scantailor RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()

if (Qt6_FOUND AND WIN32)
  #manage plugins with windeployqt.
  add_custom_command(
    TARGET scantailor
    POST_BUILD
    COMMAND ${QT_BINDIR}/windeployqt  $<TARGET_FILE:scantailor>
    COMMENT "deploy qt files alongside binary (e.g. plugins)"
  )
  install(CODE "execute_process(COMMAND \"${QT_BINDIR}/windeployqt\" \"\$\{CMAKE_INSTALL_PREFIX\}\")")
endif()

translation_sources(scantailor ${gui_only_sources} ${gui_only_ui_files})
