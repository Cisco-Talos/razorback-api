# Razorback

<p align="center">
  <img width="250" height="250" src="https://raw.githubusercontent.com/Cisco-Talos/razorback/main/logo.png" alt='The Razorback mascot'>
</p>

<p align="center">
  Razorback® is an open source framework for an intelligence driven security solution.
  malware & other malicious threats.
</p>

## Installation Instructions

We recomend running Razorback on Ubuntu LTS.

### Quick Install

Install with default options and configuration:

```bash
## If using a git checkout (not required from release tarball).
./autojunk.sh
./configure
make
make install
```

### Long/Developer Install
Debugging can be enabled with:

```./configure --enable-debug```

For all avaliable options see:

```./configure --help```


### Dependencies:

Ubuntu focal:
* automake
* automake
* autoconf
* libtool
* pkg-config
* uuid-dev
* libcurl4-openssl-dev
* libssl-dev
* libconfig-dev
* libssh-dev
* libjson-c-dev

## Linking to the Razorback API

PKG-CONFIG:
The install target installs a package metadata file in ${libdir}/pkgconfig (/usr/local/lib/pkkconfig by default).
The pkg-config utility can be used to acquire the correct CFLAGS and LDFLAGS needed to compile a nugget.

pkg-config --cflags razorback
pkg-config --libs razorback

The PKG_CHECK_MODULES macro can be used to acquire the CFLAGS and LDFLAGS from within a nugget configure script.

PKG_CHECK_MODULES([RZB], [razorback])

The above call will provide RZB_CFLAGS and RZB_LDFLAGS to use in the autoconf/automake process.

## Want to make a contribution?

The Razorback development team welcomes
[code contributions](https://github.com/Cisco-Talos/razorback-api),
improvements to
[our documentation](https://github.com/Cisco-Talos/razorback-documentation),
and also [bug reports](https://github.com/Cisco-Talos/razorback-api/issues).

Thanks for joining us!

## Licensing

Razorback is licensed for public/open source use under the GNU General Public
License, Version 2 (GPLv2).

See `LICENSE` for a copy of the license.

