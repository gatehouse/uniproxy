cmake_minimum_required(VERSION 2.8)

project(uniproxy_proj)

IF (WIN32)
	SET(VC_MAJOR "14" CACHE STRING "Visual Studio major version")
	SET(VC_MINOR "1" CACHE STRING "Visual Studio minor version")
	SET(LANG c:/lang)
ELSE()
	SET(LANG $ENV{HOME}/lang)
ENDIF()


include_directories(. src/ libs/)

IF (WIN32)

	ADD_DEFINITIONS(-D_WIN32_WINNT=0x0501 /wd4250 /wd4018  /wd4267 /wd4996 /std:c++17 /permissive- -D_HAS_AUTO_PTR_ETC /MP2)
	include_directories(${uniproxy_SOURCE_DIR} ${LANG}/w64-vc${VC_MAJOR}/include ${LANG}/cppcms-1.2.1/include ${LANG}/boost/1_73_0  ${LANG}/openssl-1.1.0e/include/)
	LINK_DIRECTORIES("${LANG}/w64-vc${VC_MAJOR}/lib"
           "${LANG}/openssl-1.1.0e/lib/"
           ${LANG}/boost/1_73_0/lib64-msvc-${VC_MAJOR}.${VC_MINOR})
        SET(CPPCMS_LIBPATH ${LANG}/cppcms-1.2.1/lib)
        SET(CPPCMS_LIBRARY
           debug "${CPPCMS_LIBPATH}/Debug/booster-d.lib" optimized "${CPPCMS_LIBPATH}/Release/booster.lib"
           debug "${CPPCMS_LIBPATH}/Debug/cppcms-d.lib" optimized "${CPPCMS_LIBPATH}/Release/cppcms.lib")
	# Use multi cores
	#SET(VISUAL_STUDIO_MP_FLAG "/MP")

ELSE (WIN32)

	ADD_DEFINITIONS(-g -std=c++17 -Wall -Wno-sign-compare)
	ADD_DEFINITIONS(-Wl,--whole-archive -Wl,--no-while-archive, -Wno-deprecated-declarations)

	include_directories(/opt/boost/1.73.0/include/ /opt/cppcms/1.2.1/include)

	LINK_DIRECTORIES(/opt/boost/1.73.0/lib/ /opt/cppcms/1.2.1/lib)

ENDIF (WIN32)


ADD_SUBDIRECTORY(libs/gatehouse gatehouse)
ADD_SUBDIRECTORY(src uniproxy)

IF (WIN32)

ELSE (WIN32)

	SET(UNIPROXY_INSTALL_PREFIX /usr/local)
	SET(CMAKE_INSTALL_PREFIX ${UNIPROXY_INSTALL_PREFIX})
	INSTALL(DIRECTORY DESTINATION ${CMAKE_INSTALL_PREFIX}/share/doc/uniproxy/)
	INSTALL(FILES script/jquery.js DESTINATION ${CMAKE_INSTALL_PREFIX}/share/doc/uniproxy/)
	INSTALL(FILES doc/license.txt DESTINATION ${CMAKE_INSTALL_PREFIX}/share/doc/uniproxy/)
	INSTALL(FILES doc/uniproxy.json.sample DESTINATION ${CMAKE_INSTALL_PREFIX}/share/doc/uniproxy/)
	INSTALL(FILES deb/uniproxy.service DESTINATION /etc/systemd/system/)

ENDIF (WIN32)

