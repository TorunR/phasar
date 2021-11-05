# Build a stand-alone executable
if (PHASAR_IN_TREE)
    add_phasar_executable(slicer
            tools/module-extractor/slicer.cpp
            tools/module-extractor/back_mapper.cpp
            tools/module-extractor/back_mapper.h
            )
else ()
    add_executable(slicer
            ../module-extractor/slicer.cpp
            ../module-extractor/back_mapper.cpp
            ../module-extractor/back_mapper.h)
endif ()

if (PHASAR_IN_TREE)
    add_phasar_executable(result_comparator
            tools/module-extractor/result_comparator.cpp
            )
else ()
    add_executable(result_comparator
            ../module-extractor/result_comparator.cpp
            )
endif ()


find_package(Boost COMPONENTS log filesystem program_options graph ${BOOST_THREAD} REQUIRED)

target_link_libraries(slicer
        LINK_PUBLIC
        phasar_config
        phasar_controller
        phasar_db
        phasar_controlflow
        phasar_ifdside
        phasar_mono
        phasar_wpds
        phasar_syncpds
        ${PHASAR_PLUGINS_LIB}
        wali
        phasar_passes
        phasar_pointer
        phasar_typehierarchy
        phasar_phasarllvm_utils
        phasar_utils
        ${Boost_LIBRARIES}
        ${CMAKE_DL_LIBS}
        ${CMAKE_THREAD_LIBS_INIT}
        ${CLANG_LIBRARY}
        )


if (USE_LLVM_FAT_LIB)
    llvm_config(slicer USE_SHARED ${LLVM_LINK_COMPONENTS})
else ()
    llvm_config(slicer ${LLVM_LINK_COMPONENTS})
endif ()

target_link_libraries(result_comparator
        LINK_PUBLIC
        phasar_config
        phasar_controller
        phasar_db
        phasar_controlflow
        phasar_ifdside
        phasar_mono
        phasar_wpds
        phasar_syncpds
        ${PHASAR_PLUGINS_LIB}
        wali
        phasar_passes
        phasar_pointer
        phasar_typehierarchy
        phasar_phasarllvm_utils
        phasar_utils
        ${Boost_LIBRARIES}
        ${CMAKE_DL_LIBS}
        ${CMAKE_THREAD_LIBS_INIT}
        ${CLANG_LIBRARY}
        )


if (USE_LLVM_FAT_LIB)
    llvm_config(slicer USE_SHARED ${LLVM_LINK_COMPONENTS})
    llvm_config(result_comparator USE_SHARED ${LLVM_LINK_COMPONENTS})
else ()
    llvm_config(slicer ${LLVM_LINK_COMPONENTS})
    llvm_config(result_comparator ${LLVM_LINK_COMPONENTS})
endif ()

set(LLVM_LINK_COMPONENTS)
