# NikonTiCtrl

Graphical user interface and API for controlling a Nikon Ti-E widefield fluorescence microscope system in the Springer lab at Harvard Medical School.

## Dependencies

### Build Dependencies
* Qt 5.15.2 (gcc 8.1.0, cmake 3.19.0)
* [Git](https://gitforwindows.org)
* [MSYS2](https://www.msys2.org) packages
    * mingw-w64-x86_64-libtiff 4.3.0-3
    * mingw-w64-x86_64-grpc 1.35.0-4
    * mingw-w64-x86_64-protobuf 3.16.0-1
* [DCAM-API](https://dcam-api.com) 21.7.6307
* [DCAM-SDK4](https://dcam-api.com/dcam-sdk-login/) 21.6.6291 (download and extract into `third_party/dcamsdk`)
* Micro-Manager 1.4.23 20180508 (for copying the Nikon Ti device adapter dll)
* NI-VISA 20.0

### Runtime Dependencies
* NI-VISA Runtime 20.0
* [DCAM-API](https://dcam-api.com) 21.7.6307 (Camera driver)
* [Ti Control](https://www.nikon.com/products/microscope-solutions/support/download/software/biological/index.htm#toc02) 4.4.6 (Microscope driver) 
* [Nikon Ti SDK Redistributable](https://micro-manager.org/wiki/NikonTI) 4.4.1.714
