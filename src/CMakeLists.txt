set(SRC_LIST
    NgxMemPool.cc
    SgiMemPool.cc)
add_library(dbmempool ${SRC_LIST})

target_link_libraries(dbmempool)