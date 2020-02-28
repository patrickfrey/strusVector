cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

include_directories(
	${Boost_INCLUDE_DIRS}
	"${Intl_INCLUDE_DIRS}"
	"${VECTOR_INCLUDE_DIRS}"
	"${MAIN_SOURCE_DIR}"
	"${strusbase_INCLUDE_DIRS}"
	"${strus_INCLUDE_DIRS}"
	${TEST_UTILS_INCLUDE_DIR}
)
link_directories(
	"${VECTOR_LIBRARY_DIRS}"
	"${strusbase_LIBRARY_DIRS}"
	${Boost_LIBRARY_DIRS}
)

add_executable( testSentenceLexer testSentenceLexer.cpp)
target_link_libraries( testSentenceLexer strus_base strus_error strus_filelocator strus_vector_std strus_database_leveldb strus_vector_testutils strus_textproc ${Boost_LIBRARIES} ${Intl_LIBRARIES} )


