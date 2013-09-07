set (INCLUDES table.h common.h trie.h)
set (SOURCES  table.c trie.c)

if (HTR_SHARED MATCHES true)
    add_library (${HTR_TARGET} SHARED ${SOURCES})
    target_link_libraries (${HTR_TARGET} ${TALLOC_TARGET})
endif ()

if (HTR_STATIC MATCHES true)
    add_library (${HTR_TARGET}_static STATIC ${SOURCES})
    set_target_properties (${HTR_TARGET}_static PROPERTIES OUTPUT_NAME ${HTR_TARGET})
    target_link_libraries (${HTR_TARGET}_static ${TALLOC_TARGET}_static)
endif ()
