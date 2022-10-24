# set(SRC_LIST
# test_pool.cc
# test_sgi.cc
# )

add_executable(test_pool test_pool.cc)
target_link_libraries(test_pool dbmempool)

add_executable(test_sgi test_sgi.cc)
target_link_libraries(test_sgi dbmempool)