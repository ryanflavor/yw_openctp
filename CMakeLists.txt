cmake_minimum_required(VERSION 3.16)

# Read LANG from .env if not specified on command line
if(NOT DEFINED LANG OR LANG STREQUAL "")
    if(EXISTS ${CMAKE_SOURCE_DIR}/.env)
        file(STRINGS ${CMAKE_SOURCE_DIR}/.env _env_lines)
        foreach(_line ${_env_lines})
            if(_line MATCHES "^LANG=(.+)$")
                set(_env_lang ${CMAKE_MATCH_1})
            endif()
        endforeach()
    endif()
    set(LANG "${_env_lang}" CACHE STRING "Which language tests to build/run: cpp, py, or all")
    if(LANG STREQUAL "")
        set(LANG "all")
    endif()
else()
    set(LANG "${LANG}" CACHE STRING "Which language tests to build/run: cpp, py, or all" FORCE)
endif()
message(STATUS "LANG = ${LANG}")

if(LANG STREQUAL "py")
    project(yw_openctp LANGUAGES NONE)
else()
    project(yw_openctp LANGUAGES CXX)
endif()

enable_testing()

if(LANG STREQUAL "cpp" OR LANG STREQUAL "all")
    add_subdirectory(cpp)

    add_test(NAME cpp_td COMMAND $<TARGET_FILE:test_td> WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/cpp)
    add_test(NAME cpp_md COMMAND $<TARGET_FILE:test_md> WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/cpp)
    set_tests_properties(cpp_td PROPERTIES
        TIMEOUT 20
        ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_SOURCE_DIR}/cpp/deps/lib")
    set_tests_properties(cpp_md PROPERTIES
        TIMEOUT 35
        ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_SOURCE_DIR}/cpp/deps/lib")
endif()

if(LANG STREQUAL "py" OR LANG STREQUAL "all")
    set(PYTEST_EXE ${CMAKE_SOURCE_DIR}/py/.venv/bin/pytest)
    if(EXISTS ${PYTEST_EXE})
        add_test(NAME py_td COMMAND ${PYTEST_EXE} test_td.py -v WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/py)
        add_test(NAME py_md COMMAND ${PYTEST_EXE} test_md.py -v WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/py)
        set_tests_properties(py_td PROPERTIES TIMEOUT 25)
        set_tests_properties(py_md PROPERTIES TIMEOUT 40)
    else()
        message(WARNING "pytest not found at ${PYTEST_EXE}, run 'cd py && uv sync && uv pip install pytest' first")
    endif()
endif()
