find_path(FREETYPE_GL_INCLUDE_DIR freetype-gl.h PATH_SUFFIXES freetype-gl)
if(WIN32)
  find_library(FREETYPE_GL_LIBRARY freetype-gl.lib)
else()
  find_library(FREETYPE_GL_LIBRARY libfreetype-gl.a)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(freetype-gl DEFAULT_MSG FREETYPE_GL_LIBRARY
                                  FREETYPE_GL_INCLUDE_DIR)

if(freetype-gl_FOUND)

  add_library(freetype-gl STATIC IMPORTED GLOBAL)
  set_target_properties(freetype-gl PROPERTIES IMPORTED_LOCATION
                                               ${FREETYPE_GL_LIBRARY})
  target_include_directories(freetype-gl SYSTEM
                             INTERFACE ${FREETYPE_GL_INCLUDE_DIR})

  add_library(freetype-gl::freetype-gl ALIAS freetype-gl)
endif()
