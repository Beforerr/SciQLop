#
# Generate the source package of SciqLop.
#

install(DIRECTORY
        ${EXECUTABLE_OUTPUT_PATH}
        DESTINATION "."
        USE_SOURCE_PERMISSIONS
        COMPONENT CORE
        PATTERN "*.a" EXCLUDE
)

set(EXECUTABLEDOTEXTENSION)
if(WIN32)
    set(EXECUTABLEDOTEXTENSION ".exe")
endif(WIN32)
set (SCIQLOP_EXE_LOCATION ${EXECUTABLE_OUTPUT_PATH}/${EXECUTABLE_NAME}${EXECUTABLEDOTEXTENSION})

if(WIN32)
    include ("cmake/sciqlop_package_qt.cmake")
endif(WIN32)


SET (CPACK_PACKAGE_VENDOR "CNRS")
SET (CPACK_PACKAGE_VERSION_MAJOR "${SCIQLOP_VERSION_MAJOR}")
SET (CPACK_PACKAGE_VERSION_MINOR "${SCIQLOP_VERSION_MINOR}")
SET (CPACK_PACKAGE_VERSION_PATCH "${SCIQLOP_VERSION_PATCH}${SCIQLOP_VERSION_SUFFIX}")
SET (CPACK_PACKAGE_VERSION "${SCIQLOP_VERSION}")
SET (CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/COPYING)
SET (CPACK_PACKAGE_CONTACT "nicolas.aunai@lpp.polytechnique.fr")
SET(CPACK_PACKAGE_DESCRIPTION_FILE ${CMAKE_CURRENT_SOURCE_DIR}/README.md)
# SET(CPACK_RESOURCE_FILE_WELCOME ${CMAKE_CURRENT_SOURCE_DIR}/WARN.txt)
SET(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/COPYING)
# SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_NAME}-${PROJECT_VERSION})
SET(FULLBUILD ON)

SET(CPACK_PACKAGE_NAME ${PROJECT_NAME})
SET(CPACK_GENERATOR "NSIS")
SET(CPACK_MONOLITHIC_INSTALL 1)
#SET(CPACK_COMPONENTS_ALL sciqlop qt)
SET(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-Setup")
SET(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_NAME} ${CPACK_PACKAGE_NAME})

set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})

if (WIN32)
   SET(CPACK_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
   SET(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
   SET(CPACK_NSIS_COMPONENT_INSTALL ON)
   SET(CPACK_SYSTEM_NAME "MinGW32")
   SET(CPACK_PACKAGING_INSTALL_PREFIX "")
   #SET(CPACK_GENERATOR "NSIS")
   SET(CPACK_NSIS_DISPLAY_NAME ${PROJECT_NAME})
   SET(CPACK_NSIS_MUI_FINISHPAGE_RUN ${SCIQLOP_EXECUTABLE_NAME})
   SET(CPACK_NSIS_MUI_ICON     ${SCIQLOP_EXECUTABLE_ICON_LOCATION})
   SET(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\${SCIQLOP_EXECUTABLE_NAME}.exe")
endif (WIN32)

INCLUDE(CPack)
