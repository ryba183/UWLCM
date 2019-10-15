Bootstrap: docker
From: nvidia/cuda:10.0-devel-ubuntu18.04

%post
export DEBIAN_FRONTEND=noninteractive
export  TZ=America/Los_Angeles
ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

apt-get update -qq \
    && apt-get install -yq --no-install-recommends \
        sudo \
        apt-utils \
        build-essential \
        pkg-config \
        git \
        ca-certificates \
        software-properties-common \
        vim \
        curl \
        language-pack-en-base

add-apt-repository ppa:ubuntu-toolchain-r/test
apt-get update

add-apt-repository ppa:ubuntu-toolchain-r/test
apt-get update -qq \
    && apt-get install -yq --no-install-recommends \
        wget \
        gnuplot-nox \
        libgnuplot-iostream-dev \
        libhdf5-dev \
        hdf5-tools \
        python-dev \
        python-h5py \
        python-numpy \
        python-scipy \
        python-matplotlib \
        python-pytest \
        libthrust-dev \
        libboost-all-dev

wget https://github.com/Kitware/CMake/releases/download/v3.13.2/cmake-3.13.2-Linux-x86_64.sh
sudo sh cmake-3.13.2-Linux-x86_64.sh --prefix=/usr/local --exclude-subdir

git clone --depth=1 git://github.com/thrust/thrust.git
sudo ln -s `pwd`/thrust/thrust /usr/local/include/thrust

git clone --depth=1 git://github.com/blitzpp/blitz.git
cd blitz
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr
make
sudo make install
cd ../../
