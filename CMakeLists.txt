cmake_minimum_required (VERSION 2.8)
project (hat-trie)

set (BOOLEANS HTR_SHARED HTR_STATIC)
foreach (bool ${BOOLEANS})
    if (NOT DEFINED ${bool})
        set (${bool} true)
    endif ()
endforeach()

if (NOT DEFINED HTR_TARGET)
    set (HTR_TARGET ${PROJECT_NAME})
endif ()

set (CMAKE_BUILD_TYPE DEBUG)

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Winline -std=gnu99")
set (CMAKE_C_FLAGS_DEBUG    "-O0 -g")

set (CMAKE_C_FLAGS_RELEASE  "-O2 -pipe -march=native -flto")
set (CMAKE_LD_FLAGS_RELEASE "-flto")

add_subdirectory (src)

include_directories ("src/")

enable_testing ()
add_subdirectory (tests)
