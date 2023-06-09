Major Changes in 0.15.2:
========================
Really minor fix release, mainly to fix a distribution issue

* Add missing file to distribution
* Fix sound recording fix in case of buffer wrapping

Major Changes in 0.15.1:
========================

* Fix some compatibility issues with FreeBSD
* Fix some minor issue with build
* Improve packaging with Meson
* Lot of C++ improves (clang-tidy)
* Fix some compatibility with no-Glibc libraries (like Musl)
* Fix minor leaks shutting down library
* Add Doxygen file to distribution
* Fix a longstanding issue related to surface updates where wrong surfaces were possibly used
* Fix compatibility with OpenSSL 3
* Updates and fixes for CI
* Use more random connection IDs to fix possible issues with proxies

Major Changes in 0.15:
======================

This is the first release in the new 0.15.x stable series. This release should
be ready for production use.

* Minor updates to CI
* Some compatibility with OpenSSL
* Change the behavior of handle_dev_start ignoring multiple start requests
* Ignore multiple calls to handle_dev_stop
* Pick up newer spice-common to fix a buffer overflow issue

Major Changes in 0.14.91:
=========================

**IMPORTANT**
0.14.91 is the first release candidate for the stable 0.15.x series. While some
bugs might still be present, it should be reasonably stable. If you are looking
for stability for daily use, please keep using the latest 0.14.x release.

* Support UNIX abstract sockets
* Fix some potential thread race condition in RedClient
* Many cleanups in the code
* Improve migration test script
* Update in protocol documentation
* Improve Meson build
* Removed CELT support
* Update CI
* Removed QXLWorker definition, it was deprecated 6 years ago
* Fix some compatibility with MacOS
* Fix some compatibility with Windows
* Move the project to C++
* Some fixes for SASL dealing with WebDAV
* Fix minor Coverity reports
* Add Doxygen support, manually built with "make doxy"
* Support more mouse buttons (up to 16 buttons)
* CVE-2020-14355 multiple buffer overflow vulnerabilities in QUIC decoding
  code

Major Changes in 0.14.3:
========================

Main changes are WebSocket and support for Windows.

* Add support for WebSocket, this will allow to use spice-html5 without proxy
* Support Windows, now Qemu Windows can be build enabling Spice
* Fix some alignment problem
* Converted some documentation to Asciidoc format to make easier to update,
  updated some
* Minor compatibility fix for PPC64EL and ARMHF
* Minor fixes for big endian machines like MIPS
* Avoid some crashes with some buggy guest drivers, simply ignore the invalid
  request
* Fix for old OpenSSL versions
* Minor fix for Windows clients and brushes, fixed an issue with Photoshop
  under Windows 7
* Add ability to query video-codecs
* Small use-after-free fix
* Fix for debugging recording/replaying using QUIC images
* Fix a regression where spice reported no monitors to the client
* Fix DoS in spicevmc if WebDAV used
* Updated and improved test migration script
* Some minor fixes to smartcard support
* Avoid possible disconnection using proxies using a in-flow keepalive
  mechanism

Major Changes in 0.14.2:
========================

Main changes are support for Meson build and graphic device info
messages allowing to better support multi-monitor configurations.

* CVE-2019-3813: fix off-by-one error in group/slot boundary check
* support H265 in stream-channel
* add support for building with meson/ninja
* minor tests fixes improving CI
* set char device state for smartcard, allowing Qemu optimization
* improve red-parse-qxl.c interface making it more consistent
* add some instrumentation for streaming device
* QXL interface: add a function to identify monitors in the guest
  (spice_qxl_set_device_info)
* add support for GraphicsDeviceInfo messages
* video-stream: prevent crash on stream reattach
* make channel client callbacks virtual functions
* bumped minimum required glib version to 2.38
* attempt to have a reliable led state for keyboard modifiers

Major Changes in 0.14.1:
========================

The main change in this release is the addition of a new protocol extension
in order to support streaming the remote display as a video stream rather than
going through the QXL protocol. Together with spice-streaming-agent, and/or with
more work on the qemu/spice-server side, this should allow streaming of 3D
accelerated VMs in the future. At this point, this part of spice-server is
still a work in progress (multi-monitor support and various features are
missing).

* add new org.spice-space.stream.0 channel used for passing an encoded video
  stream from the guest to the client
* add support for TCP_CORK to reduce the amount of packets that we send
* fix CVE-2018-10873
* fix cursor related migration crash
* fix regression causing sound recording to be muted after
  client disconnection/reconnection (introduced in 0.13.90)
* fix regression in corner cases where images could be sent uncompressed
  when they used to be compressed with QUIC
* disable TLS 1.0 support
* CELT 0.5.1 support is now disabled by default. If celt051-devel is installed
  at build-time, --enable-celt051/--disable-celt051 must be explicitly specified
* drop support for unsupported OpenSSL version. OpenSSL 1.0.0 or newer is now
  required
* bumped minimum required glib version to 2.32
* endianness fixes
* (small) leak fixes
* usual round of code cleanups
* not directly related to this release, but the upstream git repository is now
  hosted on gitlab.freedesktop.org

Major Changes in 0.14.0:
========================

This is the first release in the new 0.14.x stable series. This release should
be ready for production use.

* fix client mouse with virgl
* fix frozen display after seamless migration

Major Changes in 0.13.91:
=========================

*** IMPORTANT ***
0.13.91 is the first release candidate for the stable 0.14.x series. While some
bugs might still be present, it should be stable.

* set human-readable name on spice threads
* leak fixes
* robustness improvements (fix potential crashes in situation which should not
  happen on normal use)
* add sanity-checks for ORC library as it can abort spice-server when selinux
  is in use
* more code cleanups/improvements

Major Changes in 0.13.90:
=========================

*** IMPORTANT ***
0.13.90 is the first release candidate for the stable 0.14.x series. While some
bugs might still be present, it should be reasonably stable. If you are looking
for stability for daily use, please keep using the latest 0.12.x release.

* Close TCP connection early when client did not send the correct SPICE magic
  bytes: this allows VNC clients to gracefully fail when connecting to a SPICE
  port
* Fixes for CVE-2016-9577, CVE-2016-9578 and CVE-2017-7506
* Fix client being disconnected after migration (bug introduced in 0.13.3)
* Add a --enable-statistics configure option
* GLIB 2.28 is now a required build time dependency (used to be 2.22)
* Add VP9 encoding support when GStreamer is being used and misc
  streaming/encoding improvements
* more code refactoring/reorganization work

Major Changes in 0.13.3:
========================

*** IMPORTANT ***
The 0.13.x release series is an unstable release series, if you are looking for
stability for daily use, please use the latest 0.12.x release. We are trying to
keep the code as functional as 0.12, but regressions or bugs could still happen.

* Ongoing work on separating/encapsulating the existing codebase. Main
  highlight is that RedChannel/RedChannelClient have been converted to GObjects
* Statistics code is now disabled by default, if you need it,
  -DREDS_STATISTICS must be added to CPPFLAGS when building
* Improvements to replay utility
* Limit (deprecated/unusud) QXLMessage size to 100,000 characters for improved
  safety
* Build fixes on older distros (el6)
* Improve image quality in low bitrate situation when using the
  GStreamer backend

Major Changes in 0.13.2:
========================

*** IMPORTANT ***
The 0.13.x release series is an unstable release series, if you are looking for
stability for daily use, please use the latest 0.12.x release. We are trying to
keep the code as functional as 0.12, but regressions or bugs could still happen.

* Fixes for various CVEs
* Added GStreamer support to the video streaming code
* Fix old migration bug causing migration to never end in some cases
* Fix some vdagent-related regressions introduced in 0.13.1
* Added lz4 compression to the spicevmc channel
* Ongoing code cleanups

Major changes in 0.13.1:
========================

*** IMPORTANT ***
The 0.13.x release series is an unstable release series, if you are looking for
stability for daily use, please use the latest 0.12.x release. We are trying to
keep the code as functional as 0.12, but regressions or bugs could still happen.

* add spice_qxl_gl_scanout() spice_qxl_gl_draw_async() for local virgl support
* spice_server_set_keepalive_timeout() has been removed in favour of
  unconditionally sending keepalive probes every 10 minutes
* the rework started in 0.13.0 has moved forward. The global RedsState *reds
  variable has now been removed, GLib mainloop is now used for the RedWorker thread
  rather than an adhoc implementation, some core classes have been ported to
  GObject, ...
* some crash/leak fixes in corner cases

Major changes in 0.13.0:
========================

* Start of a large code rework to attempt to make the codebase more
  maintainable. With this first release, the huge red_worker.c file has been split
  in multiple smaller files.

* Added public spice_server_set_keepalive_timeout() to make it possible
  to tweak keepalive on all SPICE connection. This can prevent unwanted
  idle disconnections if proxies are used between the client and the host.
* Fix important memory usage when the webdav channel is used
* Do not disconnect when the client requests an unsupported compression type
* Fix potential race condition when using multiple QXL devices
* Fix display glitch when using XSpice
* Improve help string for 'replay -s'
* Fix crashes in corner cases (buggy spice-html5 + win10, vnc + SPICE port
  configured, USB webcam redirection over a slow link)
* Fix various compilation warning when building on 32 bit machines
* Do not build static libraries by default, this can be reenabled with
  --enable-static
* Fix small leak in MJPEG code

Major changes in 0.12.6:
========================
* Removed spicec client code, it has been superseded by remote-viewer and other
  spice-gtk based clients
* Unix socket support
* LZ4 support
* Let clients specify their preferred image compression format
* Allow to record and replay a spice-server session
* Fixes for CVE-2015-3247 CVE-2015-5260 and CVE-2015-5261
* spice-protocol submodule has been removed,, spice-protocol must now be
  installed when building spice-server
* Remove write polling in chardevs to reduce wakeups
* Various bugs, crashes fixes, code cleanups, ...

Major changes in 0.12.5:
========================
* Added Opus support. Celt support will be obsoleted in a future release.
* Addition of webdav channel
* Force use of TLS 1.0 or newer for TLS connections
* Reference manual
* Some optimizations improving CPU use
* Various bug fixes for race conditions, memory corruption, ... which could
  be triggered on client disconnections, migration, ... and cause spice-server
  to misbehave.
* Portability fixes
* Code cleanups

Major changes in 0.12.4:
========================
* log actual address spice-server binds to
* main_channel: fix double release of migration target data (rhbz#859027)
* red_channel: replace an assert upon threads mismatch with a warning (rhbz#823472)
* support for filtering out agent file-xfer msgs (rhbz#961848)
** new library export spice_server_set_agent_file_xfer
* mjpeg encoder statistics (mjpeg_encoder_get_stats)
* improve stream stats readability and ease of parsing
* fix for stuck display_channel over WAN (jpeg_enabled=true) (rhbz#977998)
* Use RING_FOREACH_SAFE and other SAFE macros (rhbz#887775)
* Some server/tests fixes.

Major changes in 0.12.3:
========================
* monitor client bandwidth and latency.
* dynamically adjust video stream quality based on client bandwidth & latency.
** new SPICE_MSGC_DISPLAY_STREAM_REPORT
** can also set SPICE_BIT_RATE environment variable to override.
* support arbitrary latency of audio stream wrt video stream:
** new SPICE_MSG_PLAYBACK_LATENCY
* notify agent on client disconnection
** new VD_AGENT_CLIENT_DISCONNECTED message
* better support for switching from qxl to vga mode
** new library export spice_qxl_driver_unload
* multiple monitor support in single channel fixes.
* stop streams before migration.
* don't send empty volume messages.
* Bugs fixed: rhbz#891326, rhbz#958276, rhbz#956345
* fixes to inputs, chardev, build fixes.

Major changes in 0.12.2:
========================
* Stable Release
* Skipped 0.12.1, it existed in git but was never released
* spice-server now requires glib2 (like qemu does)
* More robust ssl error and certificate handling
* Added support for websockets
* Tons of seamless migration bugfixes
* Also some none seamless migration bugfixes

Major changes in 0.12.0:
========================
* Stable Release
* support setting client monitor configuration via device
 QXLInterface::client_monitors_config
* support notifying guest of client capabilities
 QXLInterface::set_client_capabilities
* new capability for A8 Surface support
* Enable build on armv6+
* Option to quit server after first client disconnects
 spice_server_set_exit_on_disconnect

Major changes in 0.11.3:
========================
* !Development Release!
* This entry contains all 0.11.0 .. 0.11.3 changes.
* Support seamless migration: no loss of in transit messages. Still not
  supported for agent, smartcard and usb.
* Support a new rendering message, Composite, for much improved linux guest
  performance.
* Support arbitrary resolution & multiple monitors on a single display channel.
* Improved keyboard handling under network latency with new
  SPICE_MSGC_INPUTS_KEY_SCANCODE message.
* New libspice-server.so symbols:
 spice_server_set_seamless_migration
 spice_server_vm_stop
 spice_server_vm_start
 spice_qxl_monitors_config_async
* New capabilities:
 SPICE_DISPLAY_CAP_COMPOSITE
 SPICE_DISPLAY_CAP_MONITORS_CONFIG
 SPICE_INPUTS_CAP_KEY_SCANCODE
 SPICE_MAIN_CAP_AGENT_CONNECTED_TOKENS
 SPICE_MAIN_CAP_SEAMLESS_MIGRATE
* Misc:
 * char-device.c: Introducing shared flow control code for char devices
 * Enable build without client, cegui and slirp.

Major changes in 0.11.0:
========================
* !Development Release!
* 8817549..d905a1f
* now using git submodules: spice-common and spice-protocol.
* New spice protocol messages: (changes in spice-protocol, here for reference)
 * SPICE_MSG_MAIN_NAME, SPICE_MSG_MAIN_UUID
 * SPICE_MSG_DISPLAY_STREAM_DATA_SIZED
* New corresponding caps: (changes in spice-protocol, here for reference)
 * SPICE_MAIN_CAP_NAME_AND_UUID
 * SPICE_DISPLAY_CAP_SIZED_STREAM.
* Send name & uuid to capable clients
* add support for frames of different sizes RHBZ #813826
* server:
 * support a pre-opened file descriptor
 * Solaris support. Now using poll instead of epoll.
 * Support IPV6 addresses in channel events RHBZ #788444
 * other fixed RHBZ#: 787669, 787678, 819484
* spicec
 * alsa: use "default" instead of "hw:0,0"
 * volume keys support RHBZ #552539
 * other fixed RHBZ#: 78655, 804561, 641828
* solaris, mingw & windows, 32 bit fixes.
* enable server only build.
* GNULIB manywarnings.m4 & warnings.m4 module added.
* Many more bug fixes & code cleanups.
* spice-protocol no longer external.
* new server functions:
 + spice_server_set_name
 + spice_server_set_uuid
 + spice_server_set_listen_socket_fd
 + spice_server_is_server_mouse

Major changes in 0.10.1:
========================
* Mini header support
* Add server API for injecting a client connection socket
* Add Xinerama support to spicec
* Many bugfixes / code cleanups
* Requires spice-protocol >= 0.10.1

Major changes in 0.10.0:
========================
* 32 bit (little endian) server builds.
* ABI compatible with 0.8.2.

Major changes in 0.9.2:
=======================
* !Development Release!
* server: semi-seamless migration support (RHBZ 738266)
* client: semi-seamless migration support (RHBZ 725009, 738270)
* Various bugfixes / cleanups
* require spice-protocol >= 0.9.1

Major changes in 0.9.1:
=======================
* !Development Release!
* Multi-client support, disabled by default (experimental!) set the
  environment variable SPICE_DEBUG_ALLOW_MC before starting qemu to enable
* Add support for adding generic spicevmc chardev passthrough channels
* Add USB redirection channel (using generic spicevmc chardev passthrough)
* Various bugfixes / cleanups

Major changes in 0.9.0:
=======================
* !Development Release!
* volume synchronization between client and guest (client->guest only)
* turbo-jpeg used to avoid expensive color conversion in mjpeg encoder.
* Cleanups

Major changes in 0.8.2:
=======================
* server: sasl support (fdo bz 34795)
* server: support guest async io
* server: support guest suspend and hibernate
* server: add symbol versioning to libspice-server.so
* server: prevent running an old spice-server with a newer qemu
* server Bug fixes (RHBZ): 714801, 713474, 674532, 653545
* client Bug fixes (RHBZ): 712938, 710461, 673973, 667689
* require spice-protocol >= 0.8.1

Major changes in 0.8.1:
=======================
* client: Fix handling of --smartcard-db option
* client: Add --version option
* spicec-x11: Work around a bug in xsel
* spicec-x11: Don't crash on apps sending bad atoms as TARGETS
* server: Make copy paste support configurable
* server: Various fixes to agent <-> client data handling

Major changes in 0.8.0:
=======================
* client: exit nicely for --controller with no SPICE_XPI_SOCKET (rhbz#644292)
* client-x11: Use _exit rather then exit on X errors (rhbz#680763)
* client-x11: Fix keyb modifiers not syncing from guest to client (rhbz#679467)
* server: fix segfault on migration

Major changes in 0.7.3:
=======================
* Suport building with (and requires) libcacard-0.1.2
* Fixes for building with gcc-4.6
* Server: Drop unnecessary X11 and alsa requires from spice-server.pc
* Client: fix minor for old migration support
* Client: Remove spice-client watermark (rhbz#662450)

Major changes in 0.7.2:
=======================
* cmd-line-parser: fix wrong reporting of bad argument in --bla=val case
* Server: do not depend on libcacard and CEGUI (when enabled for the client)
* Server: send 1 instead of 4 as topdown flag "true" value
* Client: accept 4 as top down flag value for compatibility with older servers
* Client: stop blinking keyboard when out of focus
* Client: log subject-host mismatch, and raise ssl warnings to errors

Major changes in 0.7.1:
=======================
* Brown paper bag release
* Update SPICE_SERVER_VERSION
* Include server/tests/test_util.h in the make dist generated tarbals, so
  that they actually compile

Major changes in 0.7.0:
=======================
* Many small bugfixes to the spice client
* Support for smartcards (CAC)

Major changes in 0.6.3:
=======================
Major changes in this release:
* Foreign menu and controller support for the client for XPI / ActiveX
  browser plugin usage (same API as the 0.4 client)
* Copy and paste support in the client
* Image copy and paste support in the X client
* Fix fullscreen mode of the X client under compiz and KDE
* Various portability and bug fixes

Major changes in 0.6.2:
=======================
0.6.2 was skipped because a small but nasty bug was found while preparing
the release (and it was already tagged as 0.6.2 in git).

Major changes in 0.6.1:
=======================
Major changes in this release:
* New libspice API to handle backwards compatibility
* Fix X crash in X client
* Fix memory leaks and crashes
* Portability fixes

Major changes in 0.6.0:
=======================
Major changes in this releas:
* Various bugfixes
* Make build work on arm7
* Fix build for python 2.5
* Don't allow video streams on non-primary surface
* Fix shared memory leaks in client
* Add some new libspice-server APIs for configuration options
* Convert SpiceVDIPort API to generic SpiceCharDevice API
* Add capabilities negotiation to agent

Major changes in 0.5.3:
=======================

Major changes in this release:
* Various changes in the network protocol to make it more efficient.
* New commandline arguments to enable/disable jpeg and zlib-over-glz.
* Initial work on clipboard sharing added
* Fix color channel order for mjpegs when connecting to older spice
   server.

Major changes in 0.5.2:
=======================

This is the first release of the unstable 0.5.x series leading up to 0.6.
With this release the API of spice-server is considered stable, but
the network protocol and QXL PCI ABI are still unstable.

The major changes compared to the 0.4 series are:

* New, more efficient network protocol
* Support for offscreen surfaces in guest driver
* New spice-server API
* A marshalling/demarshalling system that isolates the network
  protocol parsing from the internal types
* A PCI parsing and validation layer making it easier to
  get backwards compatibility, cleaning up the internals ans
  makes security review easier.
* WAN support, including lossy compression using jpeg and
  zlib compression.
* Easier to build. No more dependencies on forked versions
  of pixman and cairo. Separate module spice-protocol containing
  headers used when building drivers and qemu.
