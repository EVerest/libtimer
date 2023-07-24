cmake_minimum_required(VERSION 3.11)

project(everest-timer
    VERSION 0.1
    DESCRIPTION "EVerest timer library"
    LANGUAGES CXX C
)


if (USE_FIND_PACKAGE)
    find_package(everest-cmake 0.1 REQUIRED
        PATHS ../everest-cmake
else()
    include(FetchContent)

    set(EXTERNAL_EVEREST_CMAKE_NAME everest-cmake)
    set(EXTERNAL_EVEREST_CMAKE_URL https://github.com/EVerest/everest-cmake.git)
    set(EXTERNAL_EVEREST_CMAKE_TAG v0.1.0)

    message(STATUS "Fetching everest cmake")
    FetchContent_Declare(
        ${EXTERNAL_EVEREST_CMAKE_NAME}
        GIT_REPOSITORY              ${EXTERNAL_EVEREST_CMAKE_URL}
        GIT_TAG                     ${EXTERNAL_EVEREST_CMAKE_TAG}
    )
endif()

# options
option(BUILD_EXAMPLES "Build examples" OFF)
option(TIMER_INSTALL "Install the library (shared data might be installed anyway)" ${EVC_MAIN_PROJECT})
option(CMAKE_RUN_CLANG_TIDY "Run clang-tidy" OFF)

# dependencies
find_package(Boost COMPONENTS system REQUIRED)

if (NOT DISABLE_EDM)
    evc_setup_edm()

    # In EDM mode, we can't install exports (because the dependencies usually do not install their exports)
    set(TIMER_INSTALL OFF)
else()
    if (USE_FIND_PACKAGE)
        find_package(date REQUIRED)
    else()
        set(EXTERNAL_DATE_NAME date)
        set(EXTERNAL_DATE_URL https://github.com/HowardHinnant/date.git)
        set(EXTERNAL_DATE_TAG v3.0.1)
        # Fetch Date
        FetchContent_Declare(
            ${EXTERNAL_DATE_NAME}
            GIT_REPOSITORY          ${EXTERNAL_DATE_URL}
            GIT_TAG                 ${EXTERNAL_DATE_TAG}
        )

        # Find the date package
        FetchContent_MakeAvailable(${EXTERNAL_DATE_NAME})
    endif()
endif()


# logging library
add_subdirectory(lib)

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# packaging
if (TIMER_INSTALL)
    install(
        TARGETS timer
        EXPORT timer-targets
    )

    install(
        DIRECTORY include/
        TYPE INCLUDE
    )

    evc_setup_package(
        NAME everest-timer
        EXPORT timer-targets
        NAMESPACE everest
        ADDITIONAL_CONTENT
            "find_dependency(Boost COMPONENTS system)"
    )
endif()

# build doxygen documentation if doxygen is available
find_package(Doxygen)
if(DOXYGEN_FOUND)
    set( DOXYGEN_OUTPUT_DIRECTORY dist/docs )
    doxygen_add_docs(doxygen-${PROJECT_NAME} include lib src)
else()
    message("Doxygen is needed to generate documentation")
endif()

# configure clang-tidy if requested
if(CMAKE_RUN_CLANG_TIDY)
    message("Running clang-tidy")
    string(CONCAT CLANG_TIDY_CHECKS "*,"
                                    "-llvmlibc*,"
                                    "-fuchsia-default-arguments-calls,"
                                    "-fuchsia-overloaded-operator,"
                                    "-fuchsia-statically-constructed-objects,"
                                    "-readability-function-cognitive-complexity,"
                                    "-modernize-use-trailing-return-type,"
                                    "-abseil-string-find-startswith,"
                                    "-abseil-string-find-str-contains,"
                                    ";")
    set(CMAKE_CXX_CLANG_TIDY 
    clang-tidy;
    -header-filter='.*'
    -checks=${CLANG_TIDY_CHECKS})
endif()
