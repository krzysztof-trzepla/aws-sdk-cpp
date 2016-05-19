# distro for package building (oneof: wily, fedora-23-x86_64)
DISTRIBUTION        ?= none

PKG_REVISION    ?= $(shell git describe --tags --always)
PKG_VERSION	?= $(shell git describe --tags --always | tr - .)
PKG_BUILD       ?= 1
PKG_ID           = aws-sdk-cpp-$(PKG_VERSION)

.PHONY: rpm deb

check_distribution:
ifeq ($(DISTRIBUTION), none)
	@echo "Please provide package distribution. Oneof: 'wily', 'fedora-23-x86_64'"
	@exit 1
else
	@echo "Building package for distribution $(DISTRIBUTION)"
endif

package/$(PKG_ID).tar.gz:
	mkdir -p package
	rm -rf package/$(PKG_ID)
	git archive --format=tar --prefix=$(PKG_ID)/ $(PKG_REVISION)| (cd package && tar -xf -)
	find package/$(PKG_ID) -depth -name ".git" -exec rm -rf {} \;
	tar -C package -czf package/$(PKG_ID).tar.gz $(PKG_ID)

deb: check_distribution package/$(PKG_ID).tar.gz
	rm -rf package/packages && mkdir -p package/packages
	mv -f package/$(PKG_ID).tar.gz package/aws-sdk-cpp_$(PKG_VERSION).orig.tar.gz
	cp -R pkg_config/debian package/$(PKG_ID)/
	sed -i "s/aws-sdk-cpp (.*) .*;/aws-sdk-cpp ($(PKG_VERSION)-$(PKG_BUILD)) wily;/g" package/$(PKG_ID)/debian/changelog
	sed -i "s/Build from .*/Build from $(PKG_VERSION)/g" package/$(PKG_ID)/debian/changelog

	cd package/$(PKG_ID) && sg sbuild -c "sbuild -sd $(DISTRIBUTION) -j4"
	mv package/*$(PKG_VERSION).orig.tar.gz package/packages/
	mv package/*$(PKG_VERSION)-$(PKG_BUILD).dsc package/packages/
	mv package/*$(PKG_VERSION)-$(PKG_BUILD)_amd64.changes package/packages/
	mv package/*$(PKG_VERSION)-$(PKG_BUILD).debian.tar.xz package/packages/
	mv package/*$(PKG_VERSION)-$(PKG_BUILD)*.deb package/packages/

rpm: check_distribution package/$(PKG_ID).tar.gz
	rm -rf package/packages && mkdir -p package/packages
	mv -f package/$(PKG_ID).tar.gz package/$(PKG_ID).orig.tar.gz
	cp pkg_config/aws-sdk-cpp.spec package/aws-sdk-cpp.spec
	sed -i "s/{{version}}/$(PKG_VERSION)/g" package/aws-sdk-cpp.spec
	sed -i "s/{{build}}/$(PKG_BUILD)/g" package/aws-sdk-cpp.spec

	mock --root $(DISTRIBUTION) --buildsrpm --spec package/aws-sdk-cpp.spec --resultdir=package/packages \
		--sources package/$(PKG_ID).orig.tar.gz
	mock --root $(DISTRIBUTION) --resultdir=package/packages --rebuild package/packages/$(PKG_ID)*.src.rpm

