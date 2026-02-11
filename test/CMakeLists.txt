add_executable(physwiki_diff_check physwiki_diff_check.cpp)
target_include_directories(physwiki_diff_check PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(physwiki_diff_check SQLiteCpp sqlite3 dl)
set_target_properties(physwiki_diff_check PROPERTIES
	CXX_STANDARD 11
	CXX_STANDARD_REQUIRED YES
)

add_executable(str_diff_patch2_test str_diff_patch2_test.cpp)
target_include_directories(str_diff_patch2_test PRIVATE ${PROJECT_SOURCE_DIR}/SLISC/str)
set_target_properties(str_diff_patch2_test PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
)
