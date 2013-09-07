set (TABLE       table.c str_map.c murmur_hash.c)
set (HATTRIE     hattrie.c str_map.c murmur_hash.c)
set (SORTED_ITER sorted_iter.c murmur_hash.c)

if (HTR_SHARED MATCHES true)
    add_executable (${HTR_TARGET}-table ${TABLE})
    target_link_libraries (${HTR_TARGET}-table ${HTR_TARGET})
    add_test (${HTR_TARGET}-table ${HTR_TARGET}-table)
    
    add_executable (${HTR_TARGET}-hattrie ${HATTRIE})
    target_link_libraries (${HTR_TARGET}-hattrie ${HTR_TARGET})
    add_test (${HTR_TARGET}-hattrie ${HTR_TARGET}-hattrie)
    
    add_executable (${HTR_TARGET}-sorted-iter ${SORTED_ITER})
    target_link_libraries (${HTR_TARGET}-sorted-iter ${HTR_TARGET})
    add_test (${HTR_TARGET}-sorted-iter ${HTR_TARGET}-sorted-iter)
endif ()

if (HTR_STATIC MATCHES true)
    add_executable (${HTR_TARGET}-static-table ${TABLE})
    target_link_libraries (${HTR_TARGET}-static-table ${HTR_TARGET}_static)
    add_test (${HTR_TARGET}-static-table ${HTR_TARGET}-static-table)
    
    add_executable (${HTR_TARGET}-static-hattrie ${HATTRIE})
    target_link_libraries (${HTR_TARGET}-static-hattrie ${HTR_TARGET}_static)
    add_test (${HTR_TARGET}-static-hattrie ${HTR_TARGET}-static-hattrie)
    
    add_executable (${HTR_TARGET}-static-sorted-iter ${SORTED_ITER})
    target_link_libraries (${HTR_TARGET}-static-sorted-iter ${HTR_TARGET}_static)
    add_test (${HTR_TARGET}-static-sorted-iter ${HTR_TARGET}-static-sorted-iter)
endif ()