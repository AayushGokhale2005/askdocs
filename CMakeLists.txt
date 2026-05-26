cmake_minimum_required(VERSION 3.16)
project(askdocs VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(askdocs
    src/main.cpp
    src/types.cpp
    src/terminal.cpp
    src/layout.cpp
    src/editor_view.cpp
    src/chat_panel.cpp
    src/command.cpp
    src/input_handler.cpp
    src/app.cpp
    src/utf8.cpp
    src/file_io.cpp
    src/syntax.cpp
    src/explorer.cpp
    src/tutor_service.cpp
    src/nlp/tokenizer.cpp
    src/nlp/intent_net.cpp
    src/docs/online_search.cpp
    src/docs/response_parsers.cpp
    third_party/pugixml/pugixml.cpp
    src/agent/context_buffer.cpp
    src/agent/context_agent.cpp
    src/agent/file_index.cpp
    src/agent/learning_path.cpp
    src/agent/line_analyzer.cpp
    src/agent/concept_learner.cpp
)

target_include_directories(askdocs PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/argparse
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pugixml
)

if(UNIX)
    target_link_libraries(askdocs PRIVATE util)
endif()

if(MSVC)
    target_compile_options(askdocs PRIVATE /W4)
else()
    target_compile_options(askdocs PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS askdocs RUNTIME DESTINATION bin)
