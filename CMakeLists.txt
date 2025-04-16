set(LIBJPEG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libjpeg)

add_custom_target(
  libjpeg-build
  WORKING_DIRECTORY ${LIBJPEG_DIR}
  COMMAND make CC=${CMAKE_C_COMPILER}
)

add_library(libjpeg STATIC IMPORTED)
set_property(TARGET libjpeg PROPERTY IMPORTED_LOCATION ${LIBJPEG_DIR}/.libs/libjpeg.a)

add_executable(tiny-cnn feature.cpp)

target_link_options(tiny-cnn PRIVATE -static)

target_link_libraries(tiny-cnn PRIVATE libjpeg rpmalloc-static libc-static libstdc++-static)
target_include_directories(tiny-cnn PRIVATE ${LIBJPEG_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_definitions(tiny-cnn PRIVATE CHCORE)

target_compile_options(tiny-cnn PRIVATE -mavx2)

add_custom_target(
  tiny-cnn-copy-data
  COMMAND cut -f 1 ${CMAKE_CURRENT_SOURCE_DIR}/data/filelists.txt > ${CMAKE_CURRENT_SOURCE_DIR}/data/raw_filelists.txt
  COMMAND rsync -a ${CMAKE_CURRENT_BINARY_DIR}/tiny-cnn ${build_ramdisk_dir}/
  COMMAND rsync -a ${CMAKE_CURRENT_SOURCE_DIR}/data/alexnet.dat ${build_ramdisk_dir}/
  COMMAND rsync -a ${CMAKE_CURRENT_SOURCE_DIR}/data/pca/nn-004.dat ${build_ramdisk_dir}/
  COMMAND rsync -a ${CMAKE_CURRENT_SOURCE_DIR}/image ${build_ramdisk_dir}/
  COMMAND rsync -a ${CMAKE_CURRENT_SOURCE_DIR}/data/raw_filelists.txt ${build_ramdisk_dir}/
  DEPENDS libjpeg-build tiny-cnn
)

add_dependencies(ramdisk tiny-cnn-copy-data)
add_dependencies(tiny-cnn libjpeg-build)
