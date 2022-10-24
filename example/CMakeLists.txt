set(SRC_LIST test_pool.cc)
add_executable(test_pool ${SRC_LIST})
target_link_libraries(test_pool dbmempool)