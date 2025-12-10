set(CMAKE_AUTOUIC ON)

set(ui_files
    OptionsWidget.ui)

set(sources
    ImageView.cpp ImageView.h
    Filter.cpp Filter.h
    OptionsWidget.cpp OptionsWidget.h
    Task.cpp Task.h
    CacheDrivenTask.cpp CacheDrivenTask.h
    Settings.cpp Settings.h
    Thumbnail.cpp Thumbnail.h)

add_library(finalize STATIC ${sources} ${ui_files})
target_link_libraries(finalize PUBLIC core PRIVATE dewarping output)

translation_sources(scantailor ${sources} ${ui_files})
