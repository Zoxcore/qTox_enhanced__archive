# syntax=docker/dockerfile:1
FROM ubuntu:18.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
        apt-get -y --force-yes install \
        build-essential \
        autoconf \
        automake \
        libtool \
        cmake \
        extra-cmake-modules \
        git \
        libavcodec-dev \
        libavdevice-dev \
        libavfilter-dev \
        libavutil-dev \
        libexif-dev \
        libgdk-pixbuf2.0-dev \
        libglib2.0-dev \
        libgtk2.0-dev \
        libopenal-dev \
        libopus-dev \
        libcurl4-gnutls-dev \
        libnghttp2-dev \
        libidn2-dev \
        librtmp-dev \
        libssh2-1-dev \
        libpsl-dev \
        nettle-dev \
        libgnutls28-dev \
        libgssapi-krb5-2 \
        libkrb5-dev \
        libk5crypto3 \
        libldap2-dev \
        zlib1g-dev \
        libqrencode-dev \
        libqt5opengl5-dev \
        libqt5svg5-dev \
        libsodium-dev \
        libsqlcipher-dev \
        libswresample-dev \
        libswscale-dev \
        libvpx-dev \
        libkf5sonnet-dev \
        libxss-dev \
        qt5-default \
        qttools5-dev \
        zsync \
        libarchive-dev \
        libfuse-dev \
        liblzma-dev \
        libglib2.0-dev \
        libssl-dev \
        libinotifytools0-dev \
        liblz4-dev \
        libcairo-dev \
        desktop-file-utils \
        wget \
        xxd \
        ca-certificates \
        curl \
        patchelf \
        lcov && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

COPY ./buildscripts/download/common.sh /build/download/common.sh
COPY ./buildscripts/build_utils.sh /build/build_utils.sh

COPY ./buildscripts/download/download_snore.sh /build/download/download_snore.sh
COPY ./buildscripts/build_snore_linux.sh /build/build_snore_linux.sh
COPY ./buildscripts/patches/snore.patch /build/patches/snore.patch
RUN mkdir -p /src/snore && \
    cd /src/snore && \
    /build/build_snore_linux.sh --system-install && \
    rm -fr /src/snore

COPY ./buildscripts/download/download_ldqt.sh /build/download/download_ldqt.sh
COPY ./buildscripts/build_ldqt_linux.sh /build/build_ldqt_linux.sh
RUN mkdir -p /src/ldqt && \
    cd /src/ldqt && \
    /build/build_ldqt_linux.sh && \
    rm -fr /src/ldqt

COPY ./buildscripts/download/download_aitool.sh /build/download/download_aitool.sh
COPY ./buildscripts/build_aitool_linux.sh /build/build_aitool_linux.sh
RUN mkdir -p /src/aitool && \
    cd /src/aitool && \
    /build/build_aitool_linux.sh && \
    rm -fr /src/aitool

COPY ./buildscripts/download/download_qt.sh /build/download/download_qt.sh
COPY ./buildscripts/build_qt_linux.sh /build/build_qt_linux.sh

#RUN mkdir -p /src/qt && \
#  cd /src/qt && \
#  /build/build_qt_linux.sh && \
#  rm -fr /src/qt

COPY ./buildscripts/download/download_toxcore.sh /build/download/download_toxcore.sh
COPY ./buildscripts/download/download_toxext.sh /build/download/download_toxext.sh
COPY ./buildscripts/download/download_toxext_messages.sh /build/download/download_toxext_messages.sh
COPY ./buildscripts/build_toxcore_linux.sh /build/build_toxcore_linux.sh
COPY ./buildscripts/patches/*.patch /build/patches/

RUN apt-get update && \
        apt-get -y --force-yes install \
        libx264-dev \
        libbz2-dev \
        libxv-dev \
        libsdl2-dev \
        libzmq5-dev \
        libwebp-dev \
        libssh-dev \
        libpulse-dev \
        libopenmpt-dev \
        libbs2b-dev \
        libbluray-dev \
        libass-dev \
        libcrystalhd-dev \
        libvdpau-dev \
        libva-dev \
        libva-x11-2 \
        libva-drm2 \
        libcdio-cdda-dev \
        libcdio-dev \
        libcdio-paranoia-dev \
        libsndio-dev \
        libjack-dev \
        libasound-dev \
        libxml2-dev \
        libzvbi-dev \
        libxvidcore-dev \
        libx265-dev \
        libwebpmux3 \
        libwavpack-dev \
        libvorbisenc2 \
        libvorbis-dev \
        libtwolame-dev \
        libtheora-dev \
        libspeex-dev \
        libsoxr-dev \
        libsnappy-dev \
        libshine-dev \
        librubberband-dev \
        libopenjp2-7-dev \
        libopencv-core-dev \
        libopencv-imgproc-dev \
        libmysofa-dev \
        libmp3lame-dev \
        libgsm1-dev \
        libgpgme-dev \
        flite-dev \
        libdc1394-22-dev \
        libcaca-dev \
        libraw1394-dev \
        libavc1394-dev \
        libiec61883-dev \
        libchromaprint-dev \
        librsvg2-dev \
        libgme-dev \
        && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

RUN mkdir -p /src/tox && \
    cd /src/tox && \
    /build/build_toxcore_linux.sh && \
    rm -fr /src/tox

RUN echo '/usr/local/lib/' >> /etc/ld.so.conf.d/locallib.conf && \
    ldconfig

WORKDIR /qtox
