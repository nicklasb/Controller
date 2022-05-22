FILE(GLOB_RECURSE sdp_sources ./*.*)
idf_component_register(SRCS ${sdp_sources} INCLUDE_DIRS . REQUIRES bt) 
