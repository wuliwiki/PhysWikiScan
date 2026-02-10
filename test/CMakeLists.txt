add_executable(physwiki_diff_check physwiki_diff_check.cpp)
target_include_directories(physwiki_diff_check PRIVATE ${PROJECT_SOURCE_DIR})
set_target_properties(physwiki_diff_check PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
)

add_executable(physwiki_backup_db physwiki_backup_db.cpp)
target_include_directories(physwiki_backup_db PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(physwiki_backup_db SQLiteCpp sqlite3 dl)
set_target_properties(physwiki_backup_db PROPERTIES
	CXX_STANDARD 11
	CXX_STANDARD_REQUIRED YES
)

add_executable(physwiki_backup_restore physwiki_backup_restore.cpp)
target_include_directories(physwiki_backup_restore PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(physwiki_backup_restore SQLiteCpp sqlite3 dl)
set_target_properties(physwiki_backup_restore PROPERTIES
	CXX_STANDARD 11
	CXX_STANDARD_REQUIRED YES
)
