<h1 align="center">
  Amazon Kinesis Video Streams PIC
  <br>
</h1>

<h4 align="center">Platform Indendent Code for Amazon Kinesis Video Streams </h4>

<p align="center">
  <a href="https://github.com/awslabs/amazon-kinesis-video-streams-pic/actions/workflows/ci.yml"> <img src=https://github.com/awslabs/amazon-kinesis-video-streams-pic/actions/workflows/ci.yml/badge.svg </a>
  <a href="https://codecov.io/gh/awslabs/amazon-kinesis-video-streams-pic"> <img src="https://codecov.io/gh/awslabs/amazon-kinesis-video-streams-pic/branch/master/graph/badge.svg" alt="Coverage Status"> </a>
</p>

<p align="center">
  <a href="#key-features">Key Features</a> •
  <a href="#build">Build</a> •
  <a href="#run">Run</a> •
  <a href="#documentation">Documentation</a> •
  <a href="#related">Related</a> •
  <a href="#license">License</a>
</p>

## Key Features

## Build
### Download
To download run the following command:

`git clone https://github.com/awslabs/amazon-kinesis-video-streams-pic.git`

You will also need to install `pkg-config` and `CMake` and a build enviroment

### Configure
Create a build directory in the newly checked out repository, and execute CMake from it.

`mkdir -p amazon-kinesis-video-streams-pic/build; cd amazon-kinesis-video-streams-pic/build; cmake .. `

By default we download all the libraries from GitHub and build them locally, so should require nothing to be installed ahead of time.
If you do wish to link to existing libraries you can use the following flags to customize your build.

#### Cross-Compilation

If you wish to cross-compile `CC` and `CXX` are respected when building the library and all its dependencies. See our [.travis.yml](.travis.yml) for an example of this. Every commit is cross compiled to ensure that it continues to work.


#### CMake Arguments
You can pass the following options to `cmake ..`

* `-DBUILD_DEPENDENCIES` -- Whether or not to build depending libraries from source
* `-DBUILD_TEST=TRUE` -- Build unit/integration tests, may be useful for confirm support for your device. `./kvspic_test`
* `-DCODE_COVERAGE` --  Enable coverage reporting
* `-DCOMPILER_WARNINGS` -- Enable all compiler warnings
* `-DADDRESS_SANITIZER` -- Build with AddressSanitizer
* `-DMEMORY_SANITIZER` --  Build with MemorySanitizer
* `-DTHREAD_SANITIZER` -- Build with ThreadSanitizer
* `-DUNDEFINED_BEHAVIOR_SANITIZER` Build with UndefinedBehaviorSanitizer
* `-DBUILD_DEBUG_HEAP` Build debug heap with guard bands and validation. This is ONLY intended for low-level debugging purposes. Default is OFF
* `-DALIGNED_MEMORY_MODEL` Build for aligned memory model only devices. Default is OFF.

### Build
To build the library run make in the build directory you executed CMake.

`make`

### Note on alignment

The entire PIC codebase is built with aligned memory access to machine native word (up-to 64 bit). The only exception is the heap implementation. In order to provide for tight packing and low-fragmentation, we default to unaligned heap access. For devices and OS-es that do not have unaligned access or unaligned access emulation, `-DALIGNED_MEMORY_MODEL` CMake argument should be passed in build-time to ensure heap is aligned.

## Contributing to this project

If you wish to submit a pull request to this project, please do so on the `develop` branch. Code from develop will be merged into master periodically as a part of our release cycle.

## Documentation

## Related

## License

This library is licensed under the Apache 2.0 License.
