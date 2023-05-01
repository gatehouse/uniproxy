cmake_minimum_required(VERSION 3.10)

project(uniproxy_proj)

IF (WIN32)
	SET(CPPCMS_DIR c:/local/cppcms-2.0.0)
	SET(BOOST_DIR c:/local/boost_1_82_0)
	SET(OPENSSL_DIR c:/local/openssl-3.1.0)
ELSE()
	SET(LANG $ENV{HOME}/lang)
	SET(BOOST_DIR /opt/boost/1.76.0)
	SET(CPPCMS_DIR /opt/cppcms/2.0.0)
ENDIF()

include_directories(. src/ libs/)

IF (WIN32)

	ADD_DEFINITIONS(-D_WIN32_WINNT=0x0501 /wd4250 /wd4018  /wd4267 /wd4996 /std:c++17 /permissive- -D_HAS_AUTO_PTR_ETC /MP2)
	include_directories(${uniproxy_SOURCE_DIR} ${CPPCMS_DIR}/include ${BOOST_DIR} "${OPENSSL_DIR}/include")
	link_directories(${CPPCMS_DIR}/lib ${BOOST_DIR}/lib64-msvc-14.3 "${OPENSSL_DIR}/lib")

ELSE (WIN32)

	ADD_DEFINITIONS(-g -std=c++17 -Wall -Wno-sign-compare)
	ADD_DEFINITIONS(-Wl,--whole-archive -Wl,--no-while-archive, -Wno-deprecated-declarations)

	include_directories(${BOOST_DIR}/include/ ${CPPCMS_DIR}/include/ ${CPPCMS_DIR}/booster/)

	LINK_DIRECTORIES(${BOOST_DIR}/lib/ ${CPPCMS_DIR}/lib/)

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
	#INSTALL(FILES deb/uniproxy.service DESTINATION /etc/systemd/system/)

ENDIF (WIN32)

