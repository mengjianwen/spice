NULL =
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = subprojects/spice-common server docs tools

if ENABLE_TESTS
check-valgrind:
	$(MAKE) -C server check-valgrind
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = spice-server.pc

DISTCHECK_CONFIGURE_FLAGS =			\
	--enable-smartcard			\
	--with-sasl				\
	--enable-manual				\
	$(NULL)

EXTRA_DIST =					\
	meson.build				\
	meson_options.txt			\
	build-aux/git-version-gen		\
	build-aux/meson-dist			\
	build-aux/meson/check-spice-common	\
	CHANGELOG.md				\
	doxygen.sh				\
	doxygen.cfg				\
	.version				\
	$(NULL)

# see git-version-gen
dist-hook:
	echo $(VERSION) > $(distdir)/.tarball-version

BUILT_SOURCES = $(top_srcdir)/.version
$(top_srcdir)/.version:
	echo $(VERSION) > $@-t && mv $@-t $@

doxy:
	$(srcdir)/doxygen.sh
