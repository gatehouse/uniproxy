cmake_minimum_required(VERSION 3.10)

project(uniproxy_proj)

OPTION(OPT_SYSTEMD "Use systemd on Linux" ON)

IF (WIN32)
	#SET(VC_MAJOR "17" CACHE STRING "Visual Studio major version")
	#SET(VC_MINOR "2" CACHE STRING "Visual Studio minor version")
	SET(LANG c:/lang)
	SET(CPPCMS_DIR ${LANG}/cppcms-2.0.0.beta2)
	SET(BOOST_DIR ${LANG}/boost/1_81_0)
	SET(OPENSSL_DIR ${LANG}/openssl-3.1.0)
	SET(LIBS_DIR ${LANG}/w64-vc17)
ELSE()
	SET(LANG $ENV{HOME}/lang)
ENDIF()


include_directories(. src/ libs/)

IF (WIN32)

	ADD_DEFINITIONS(-D_WIN32_WINNT=0x0501 /wd4250 /wd4018  /wd4267 /wd4996 /std:c++17 /permissive- -D_HAS_AUTO_PTR_ETC /MP2)
	include_directories(${uniproxy_SOURCE_DIR} ${LIBS_DIR}/include ${BOOST_DIR} "${OPENSSL_DIR}/include")
	link_directories(${LIBS_DIR}/lib ${BOOST_DIR}/lib64-msvc-14.3 "${OPENSSL_DIR}/lib")
	#c:/lang/w64-vc19/lib/ c:/lang/boost_1_81_0/lib64-msvc-14.3/ "c:/program files/openssl-win64/lib")
	#include_directories(${uniproxy_SOURCE_DIR} ${LANG}/w64-vc${VC_MAJOR}/include c:/lang/boost_1_81_0/ "c:/program files/openssl-win64/include/")
	#link_directories(c:/lang/w64-vc19/lib/ c:/lang/boost_1_81_0/lib64-msvc-14.3/ "c:/program files/openssl-win64/lib")



	#LINK_DIRECTORIES("${LANG}/w64-vc${VC_MAJOR}/lib"
    #       "${LANG}/openssl-1.1.1t/lib/"
    #       ${LANG}/boost/1_73_0/lib64-msvc-${VC_MAJOR}.${VC_MINOR})
    #    SET(CPPCMS_LIBPATH ${LANG}/cppcms-1.2.1/lib)
    #    SET(CPPCMS_LIBRARY
    #       debug "${CPPCMS_LIBPATH}/Debug/booster-d.lib" optimized "${CPPCMS_LIBPATH}/Release/booster.lib"
    #       debug "${CPPCMS_LIBPATH}/Debug/cppcms-d.lib" optimized "${CPPCMS_LIBPATH}/Release/cppcms.lib")
	# Use multi cores
	#SET(VISUAL_STUDIO_MP_FLAG "/MP")

ELSE (WIN32)

	ADD_DEFINITIONS(-g -std=c++17 -Wall -Wno-sign-compare)
	ADD_DEFINITIONS(-Wl,--whole-archive -Wl,--no-while-archive, -Wno-deprecated-declarations)
        IF (OPT_SYSTEMD)
	   ADD_DEFINITIONS(-D_SYSTEMD_)
        ENDIF()

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
	INSTALL(FILES LICENSE.txt DESTINATION ${CMAKE_INSTALL_PREFIX}/share/doc/uniproxy/)
	INSTALL(FILES doc/uniproxy.json.sample DESTINATION ${CMAKE_INSTALL_PREFIX}/share/doc/uniproxy/)
	INSTALL(FILES deb/uniproxy.service DESTINATION /etc/systemd/system/)

ENDIF (WIN32)

