FROM ubuntu:jammy

ARG DEBIAN_FRONTEND=noninteractive

RUN rm /bin/sh && ln -s /bin/bash /bin/sh

#
# Uniproxy dependencies
#
RUN apt-get update && apt-get install -y openssl

WORKDIR /tmp

COPY *.deb ./

RUN dpkg -i *.deb
RUN rm -f *

VOLUME /var/lib/uniproxy

EXPOSE 1240
EXPOSE 8085
EXPOSE 25500

USER uniproxy

WORKDIR /var/lib/uniproxy
CMD /usr/local/bin/uniproxy

