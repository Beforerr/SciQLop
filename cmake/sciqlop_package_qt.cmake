STRING(TOLOWER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE_TO_LOWER)
SET(WINDEPLOYQT_ARGS --${CMAKE_BUILD_TYPE_TO_LOWER} --list mapping -network -sql --no-system-d3d-compiler --compiler-runtime --dry-run)

#FOREACH(qtModule @_qt5_modules@)
# STRING(TOLOWER ${qtModule} qtLowerModule)
# SET(WINDEPLOYQT_ARGS ${WINDEPLOYQT_ARGS} -${qtLowerModule})
#ENDFOREACH()

EXECUTE_PROCESS(
COMMAND windeployqt ${WINDEPLOYQT_ARGS} -printsupport ${SCIQLOP_EXE_LOCATION}
OUTPUT_VARIABLE QT_FILES
)

IF( QT_FILES )

STRING(REPLACE "\n" ";" QT_FILES ${QT_FILES})
LIST(APPEND QT_FILES_LIST ${QT_FILES})
FOREACH(QtFile ${QT_FILES_LIST})
 STRING(REPLACE "\"" "" QtFile ${QtFile})
 STRING(REPLACE "\\" "/" QtFile ${QtFile})
 STRING(REGEX MATCH "^(.*) " QtFileSource ${QtFile})
 STRING(REGEX MATCH " (.*)$" QtFileTarget ${QtFile})
 
 STRING(STRIP ${QtFileSource} QtFileSource)
 STRING(STRIP ${QtFileTarget} QtFileTarget)
 GET_FILENAME_COMPONENT(QtFileTargetDir ${QtFileTarget} DIRECTORY)
  
 IF(NOT EXISTS "${CMAKE_INSTALL_PREFIX}/${EXECUTABLE_OUTPUT_PATH}/${QtFileTarget}")
   GET_FILENAME_COMPONENT(QtFileTargetDir ${QtFileTarget} DIRECTORY)
   FILE(INSTALL DESTINATION "${EXECUTABLE_OUTPUT_PATH}/${QtFileTargetDir}" FILES "${QtFileSource}")
 ENDIF()
ENDFOREACH()

ENDIF()

MESSAGE( "Exec windeployqt done" )
