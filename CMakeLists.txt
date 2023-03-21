cmake_minimum_required(VERSION 3.10)

project(uniproxy_proj)


IF (WIN32)
	SET(LANG c:/lang)
	SET(CPPCMS_DIR ${LANG}/cppcms-2.0.0.beta2)
	SET(BOOST_DIR ${LANG}/boost/1_81_0)
	SET(OPENSSL_DIR ${LANG}/openssl-3.1.0)
	SET(LIBS_DIR ${LANG}/w64-vc17)
ELSE()
	SET(LANG $ENV{HOME}/lang)
	SET(BOOST_DIR /opt/boost/1.81.0)
	SET(CPPCMS_DIR cppcms)
   ##/opt/cppcms/1.2.1)
ENDIF()


include_directories(. src/ libs/)

IF (WIN32)

	ADD_DEFINITIONS(-D_WIN32_WINNT=0x0501 /wd4250 /wd4018  /wd4267 /wd4996 /std:c++17 /permissive- -D_HAS_AUTO_PTR_ETC /MP2)
	include_directories(${uniproxy_SOURCE_DIR} ${LIBS_DIR}/include ${BOOST_DIR} "${OPENSSL_DIR}/include")
	link_directories(${LIBS_DIR}/lib ${BOOST_DIR}/lib64-msvc-14.3 "${OPENSSL_DIR}/lib")

ELSE (WIN32)

	ADD_DEFINITIONS(-g -std=c++17 -Wall -Wno-sign-compare)
	ADD_DEFINITIONS(-Wl,--whole-archive -Wl,--no-while-archive, -Wno-deprecated-declarations)

   #OPTION(OPT_SYSTEMD "Use systemd on Linux" ON)
   #   IF (OPT_SYSTEMD)
   #	   ADD_DEFINITIONS(-D_SYSTEMD_)
   #   ENDIF()

	include_directories(${BOOST_DIR}/include/ ${CPPCMS_DIR} ${CPPCMS_DIR}/booster ${CPPCMS_DIR}/build/ ${CPPCMS_DIR}/build/booster/)

	LINK_DIRECTORIES(${BOOST_DIR}/lib/ ${CPPCMS_DIR}/build/ ${CPPCMS_DIR}/build/booster/)

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

