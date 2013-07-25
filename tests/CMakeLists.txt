set (AHTABLE     ahtable.c str_map.c)
set (HATTRIE     hattrie.c str_map.c)
set (SORTED_ITER sorted_iter.c)

if (HTR_SHARED MATCHES true)
    add_executable (${HTR_TARGET}-ahtable ${AHTABLE})
    target_link_libraries (${HTR_TARGET}-ahtable ${HTR_TARGET})
    add_test (${HTR_TARGET}-ahtable ${HTR_TARGET}-ahtable)
    
    add_executable (${HTR_TARGET}-hattrie ${AHTABLE})
    target_link_libraries (${HTR_TARGET}-hattrie ${HTR_TARGET})
    add_test (${HTR_TARGET}-hattrie ${HTR_TARGET}-hattrie)
    
    add_executable (${HTR_TARGET}-sorted-iter ${SORTED_ITER})
    target_link_libraries (${HTR_TARGET}-sorted-iter ${HTR_TARGET})
    add_test (${HTR_TARGET}-sorted-iter ${HTR_TARGET}-sorted-iter)
endif ()

if (HTR_STATIC MATCHES true)
    add_executable (${HTR_TARGET}-static-ahtable ${AHTABLE})
    target_link_libraries (${HTR_TARGET}-static-ahtable ${HTR_TARGET}_static)
    add_test (${HTR_TARGET}-static-ahtable ${HTR_TARGET}-static-ahtable)
    
    add_executable (${HTR_TARGET}-static-hattrie ${HATTRIE})
    target_link_libraries (${HTR_TARGET}-static-hattrie ${HTR_TARGET}_static)
    add_test (${HTR_TARGET}-static-hattrie ${HTR_TARGET}-static-hattrie)
    
    add_executable (${HTR_TARGET}-static-sorted-iter ${SORTED_ITER})
    target_link_libraries (${HTR_TARGET}-static-sorted-iter ${HTR_TARGET}_static)
    add_test (${HTR_TARGET}-static-sorted-iter ${HTR_TARGET}-static-sorted-iter)
endif ()