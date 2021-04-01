set(SlicerTestSources
        SliceITCW_defectsTest.cpp
        Parson_test.cpp)

foreach(TEST_SRC ${SlicerTestSources})
    add_phasar_unittest(${TEST_SRC})
endforeach(TEST_SRC)
