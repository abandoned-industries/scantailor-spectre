set(CMAKE_AUTOUIC ON)

set(ui_files
    OptionsWidget.ui)

set(sources
    Filter.cpp Filter.h
    Task.cpp Task.h
    CacheDrivenTask.cpp CacheDrivenTask.h
    Dependencies.cpp Dependencies.h
    ImageView.cpp ImageView.h
    OrderByWidthProvider.cpp OrderByWidthProvider.h
    OrderByHeightProvider.cpp OrderByHeightProvider.h
    PageFinder.cpp PageFinder.h
    Params.cpp Params.h
    Settings.cpp Settings.h
    Thumbnail.cpp Thumbnail.h
    OptionsWidget.cpp OptionsWidget.h)

add_library(page_box STATIC ${sources} ${ui_files})
target_link_libraries(page_box PUBLIC core)

translation_sources(scantailor ${sources} ${ui_files})
