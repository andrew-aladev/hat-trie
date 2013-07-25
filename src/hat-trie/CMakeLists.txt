set (INCLUDES ahtable.h common.h hat-trie.h murmurhash3.h pstdint.h)
set (SOURCES  ahtable.c hat-trie.c murmurhash3.c)

if (HTR_SHARED MATCHES true)
    add_library (${HTR_TARGET} SHARED ${SOURCES})
    target_link_libraries (${HTR_TARGET})
endif ()

if (HTR_STATIC MATCHES true)
    add_library (${HTR_TARGET}_static STATIC ${SOURCES})
    set_target_properties (${HTR_TARGET}_static PROPERTIES OUTPUT_NAME ${HTR_TARGET})
    target_link_libraries (${HTR_TARGET}_static)
endif ()
