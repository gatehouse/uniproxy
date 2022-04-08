FROM ubuntu:focal

ARG DEBIAN_FRONTEND=noninteractive

RUN rm /bin/sh && ln -s /bin/bash /bin/sh

COPY run-uniproxy.sh ./

#
# Uniproxy dependencies
#
RUN apt-get install -y openssl

WORKDIR /tmp

COPY *.deb ./

RUN dpkg -i *.deb

#
# run uniproxy
#
USER root

VOLUME /var/lib/uniproxy

WORKDIR /var/lib/uniproxy
EXPOSE 1240
EXPOSE 8085
EXPOSE 25500
CMD run-uniproxy.sh
