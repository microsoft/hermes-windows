include(FetchContent)

FetchContent_Declare(
  folly
  URL      https://github.com/facebook/folly/archive/v2020.01.13.00.tar.gz
  URL_HASH SHA256=b851e02310536cefbbc1e7bbcc2dfedb277bf9f4f87c1bed1dc4d7a22f574fa4
)

FetchContent_GetProperties(folly)

include(CMakePrintHelpers)

if(NOT folly_POPULATED)
  FetchContent_Populate(folly)

  file(READ ${folly_SOURCE_DIR}/folly/portability/Builtins.h file_text)
  string(REPLACE
    "#if !defined(_MSC_VER) || (_MSC_VER < 1923)"
    "#if !defined(_MSC_VER) || _MSC_VER < 1923 || _MSC_VER >= 1928"
    file_text
    "${file_text}"
  )
  file(WRITE ${folly_SOURCE_DIR}/folly/portability/Builtins.h "${file_text}")
endif()

set(FOLLY_SOURCE ${folly_SOURCE_DIR} PARENT_SCOPE)
