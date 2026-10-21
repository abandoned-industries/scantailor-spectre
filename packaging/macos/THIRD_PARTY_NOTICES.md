# Third-party software in 2.0b7

ScanTailor Spectre is GPLv3. This Apple Silicon package dynamically links Qt 6.8.3 under the open-source licenses supplied here. Qt and Qt WebEngine source, including their third-party notices, are available at https://download.qt.io/archive/qt/6.8/6.8.3/submodules/ . Qt WebEngine Chromium notices can also be viewed via its chrome://credits resource. Source builds below target macOS15; Qt official binaries retain their original deployment minima.

Do not treat this package as runtime-tested on macOS15: functional verification is macOS27 only.

- jpeg-turbo 3.2.0 (IJG AND Zlib AND BSD-3-Clause): https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/3.2.0/libjpeg-turbo-3.2.0.tar.gz — SHA-256 `6f30092cef9fb839779646608f4ee14ae3cbac989c47fa05e841b0841f09878e`
- libpng 1.6.59 (libpng-2.0): https://downloads.sourceforge.net/project/libpng/libpng16/1.6.59/libpng-1.6.59.tar.xz — SHA-256 `d80dd2a38a37f803cb9b6ac7b14bd6e74ddc3b654780a8380bdf93523fdb4389`
- libtiff 4.7.2 (libtiff): https://download.osgeo.org/libtiff/tiff-4.7.2.tar.gz — SHA-256 `672bd7d10aee4606171afb864f3570b83340f6a33e2c186dc0512f7145ffdf6a`
- leptonica 1.87.0 (BSD-2-Clause): https://github.com/DanBloomberg/leptonica/releases/download/1.87.0/leptonica-1.87.0.tar.gz — SHA-256 `c73363397f96eb1295602bf44d708a994ad42046c791bf03ea0505d829bdb6a7`
- webp 1.6.0 (BSD-3-Clause): https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-1.6.0.tar.gz — SHA-256 `e4ab7009bf0629fd11982d4c2aa83964cf244cffba7347ecd39019a9e38c4564`
- openjpeg 2.5.4 (BSD-2-Clause): https://github.com/uclouvain/openjpeg/archive/refs/tags/v2.5.4.tar.gz — SHA-256 `a695fbe19c0165f295a8531b1e4e855cd94d0875d2f88ec4b61080677e27188a`
- giflib 6.1.3 (MIT): https://downloads.sourceforge.net/project/giflib/giflib-6.x/giflib-6.1.3.tar.gz — SHA-256 `b65b66b99f0424b93525f987386f22fc5efb9da2bfc92ad4a532249aaffbab0e`
- xz 5.8.4 (0BSD AND GPL-2.0-or-later): https://github.com/tukaani-project/xz/releases/download/v5.8.4/xz-5.8.4.tar.gz — SHA-256 `0014c7886930454fe8bd4228665b51af55eeae560ea135c9c4cd33f55b2591d9`
- zstd 1.5.7 ((BSD-3-Clause OR GPL-2.0-only) AND BSD-2-Clause AND MIT): https://github.com/facebook/zstd/archive/refs/tags/v1.5.7.tar.gz — SHA-256 `37d7284556b20954e56e1ca85b80226768902e2edabd3b649e9e72c0c9012ee3`

Apple system frameworks, Swift runtime and zlib are supplied by macOS, not redistributed from Homebrew. Qt archive provenance and package-audit scripts accompany the source release.
