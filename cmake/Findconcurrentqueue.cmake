add_library(concurrentqueue INTERFACE IMPORTED GLOBAL)
target_include_directories(concurrentqueue SYSTEM
                           INTERFACE external/concurrentqueue)
target_compile_features(concurrentqueue INTERFACE cxx_std_11)

add_library(concurrentqueue::concurrentqueue ALIAS concurrentqueue)
