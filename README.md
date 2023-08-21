# Multichannel AWG

This example illustrates the usage of modern UHD4 features to sequence transmissions.

## Installation

### Building

```shell
# The installation target directory
# Note that you can also install globally instead of to your home directory
# in that case, you'd want something like targetdir=/usr/local or just /usr.
targetdir=$HOME/.local/bin

mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=${targetdir} ..
make
```

### Installing – Development

After building, run from the `build/` directory

```shell
make install
# if you installed to a directory your user can't write to,
# `sudo make install` instead
```

### Installing – Deployment

You can build a binary package for transfer to other machines:

```shell
# RPM is for Fedora/Rocky/Alma/CentOS/SuSE/Redhat…
# DEB for debian/Mint/Ubuntu
# NSIS64 for a Windows installer (untested, but should work, needs preparation)
generator=RPM
mkdir build
cd build
cmake ..
make
cpack -G 
```
