# KMX Bank development image.
#
# The upstream Qt 6.11 image ships GCC 13, which tops out at C++23. clang-20 is
# added here because the project builds at C++26 (see the standard detection in
# CMakeLists.txt); Qt itself is still the GCC-built one, which is fine on Linux
# where both compilers share libstdc++ and the Itanium ABI.
FROM dalogik/qt-docker:qt6.11.0-linux64-gcc

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        clang-20 \
        clang-format-20 \
    && rm -rf /var/lib/apt/lists/*

ENV CC=/usr/bin/clang-20 \
    CXX=/usr/bin/clang++-20
