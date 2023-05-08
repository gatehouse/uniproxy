FROM ubuntu:bionic AS uniproxy_build

ENV DEBIAN_FRONTEND=noninteractive
ENV VERSION_CPPCMS=2.0.0
ENV VERSION_CPPCMS_GIT=v2.0.0.beta2

RUN apt-get update
RUN apt install -y \
    software-properties-common \
    wget \
    checkinstall \
    cmake \
    git \
    g++ \
    python \
    python-dev \
    python3 \
    python3-dev \
    libssl-dev \
    libgcrypt20-dev \
    libicu-dev \
    zlib1g-dev \
    libpcre++-dev

WORKDIR /libboost_source
RUN wget --quiet -O boost_1_76_0.tar.gz https://sourceforge.net/projects/boost/files/boost/1.76.0/boost_1_76_0.tar.gz/download \
    && tar xzf boost_1_76_0.tar.gz \
    && cd boost_1_76_0 \
    && ./bootstrap.sh --prefix=/opt/boost/1.76.0 \
    && ./b2 --prefix=/opt/boost/1.76.0 install

WORKDIR /cppcms_source
RUN git clone https://github.com/artyom-beilis/cppcms.git .
RUN git checkout ${VERSION_CPPCMS_GIT}

WORKDIR /cppcms_source/build
RUN cmake .. -DCMAKE_INSTALL_PREFIX=/opt/cppcms/${VERSION_CPPCMS}
RUN make -j 4
RUN make install

WORKDIR /uniproxy_build
ADD . /uniproxy_build

WORKDIR /uniproxy_build/build
RUN cmake ..
RUN make -j 4

RUN ln -s ../deb/postinstall-pak \
    && ln -s ../deb/preinstall-pak \
    && ln -s ../deb/preremove-pak \
    && ln -s ../deb/postremove-pak \
    && ln -s ../deb/description-pak

RUN export RELEASE_VERSION=`sed -n -e 's/const.*version.*"\(.*\)";/\1/p' ../release.cpp` \
    && checkinstall \
    --install=no \
    --pkgversion=\${RELEASE_VERSION} \
    --pkgrelease=1 \
    --pkglicense=GPL \
    --pkgname=uniproxy \
    --maintainer='GateHouse \<support@gatehouse.com\>' \
    --provides=uniproxy \
    --requires='openssl' \
    --nodoc \
    --exclude=/home  \
    --backup=no \
    -y