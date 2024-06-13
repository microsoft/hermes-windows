include(FetchContent)

set(boostComponents
  algorithm
  asio
  assert
  bind
  concept_check
  config
  container_hash
  core
  detail
  function
  integer
  iterator
  move
  mpl
  preprocessor
  range
  static_assert
  throw_exception
  type_index
  type_traits
  utility
  variant
)

foreach(component ${boostComponents})
  FetchContent_Declare(
    boost_${component}
    GIT_REPOSITORY https://github.com/boostorg/${component}.git
    GIT_TAG        boost-1.76.0
    GIT_SHALLOW    1
    GIT_PROGRESS   1
  )

  FetchContent_GetProperties(boost_${component})

  if(NOT boost_${component}_POPULATED)
    FetchContent_Populate(boost_${component})
  endif()

  list(APPEND boostIncludes ${boost_${component}_SOURCE_DIR}/include)
endforeach()

set(BOOST_INCLUDES ${boostIncludes} PARENT_SCOPE)
