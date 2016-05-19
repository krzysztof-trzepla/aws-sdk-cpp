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

%/$(PKG_ID).tar.gz:
	mkdir -p $*
	rm -rf $*/$(PKG_ID)
	git archive --format=tar --prefix=$(PKG_ID)/ $(PKG_REVISION)| (cd $* && tar -xf -)
	find $*/$(PKG_ID) -depth -name ".git" -exec rm -rf {} \;
	tar -C $* -czf $*/$(PKG_ID).tar.gz $(PKG_ID)

deb:
	mkdir -p package/$(DISTRIBUTION)/source package/$(DISTRIBUTION)/binary-amd64
	./bamboos/make.py -e DISTRIBUTION=$(DISTRIBUTION) --privileged --group sbuild -i onedata/deb_builder package_deb

package_deb: check_distribution deb/$(PKG_ID).tar.gz
	rm -rf deb/packages && mkdir -p deb/packages
	mv -f deb/$(PKG_ID).tar.gz deb/aws-sdk-cpp_$(PKG_VERSION).orig.tar.gz
	cp -R pkg_config/debian deb/$(PKG_ID)/
	sed -i "s/aws-sdk-cpp (.*) .*;/aws-sdk-cpp ($(PKG_VERSION)-$(PKG_BUILD)) wily;/g" deb/$(PKG_ID)/debian/changelog
	sed -i "s/Build from .*/Build from $(PKG_VERSION)/g" deb/$(PKG_ID)/debian/changelog

	cd deb/$(PKG_ID) && sg sbuild -c "sbuild -sd $(DISTRIBUTION) -j4"

	mv deb/*$(PKG_VERSION).orig.tar.gz package/$(DISTRIBUTION)/source
	mv deb/*$(PKG_VERSION)-$(PKG_BUILD).dsc package/$(DISTRIBUTION)/source
	mv deb/*$(PKG_VERSION)-$(PKG_BUILD).diff.gz package/$(DISTRIBUTION)/source || \
	mv deb/*$(PKG_VERSION)-$(PKG_BUILD).debian.tar.xz package/$(DISTRIBUTION)/source
	mv deb/*$(PKG_VERSION)-$(PKG_BUILD)_amd64.changes package/$(DISTRIBUTION)/source
	mv deb/*$(PKG_VERSION)-$(PKG_BUILD)*.deb package/$(DISTRIBUTION)/binary-amd64

rpm:
	mkdir -p package/$(DISTRIBUTION)/SRPMS package/$(DISTRIBUTION)/x86_64
	./bamboos/make.py -e DISTRIBUTION=$(DISTRIBUTION) --privileged --group mock -i onedata/rpm_builder package_rpm

package_rpm: check_distribution rpm/$(PKG_ID).tar.gz
	rm -rf rpm/packages && mkdir -p rpm/packages
	mv -f rpm/$(PKG_ID).tar.gz rpm/$(PKG_ID).orig.tar.gz
	cp pkg_config/aws-sdk-cpp.spec rpm/aws-sdk-cpp.spec
	sed -i "s/{{version}}/$(PKG_VERSION)/g" rpm/aws-sdk-cpp.spec
	sed -i "s/{{build}}/$(PKG_BUILD)/g" rpm/aws-sdk-cpp.spec

	mock --root $(DISTRIBUTION) --buildsrpm --spec rpm/aws-sdk-cpp.spec --resultdir=rpm/packages \
		--sources rpm/$(PKG_ID).orig.tar.gz
	mock --root $(DISTRIBUTION) --resultdir=rpm/packages --rebuild rpm/packages/$(PKG_ID)*.src.rpm

	mv rpm/packages/*.src.rpm package/$(DISTRIBUTION)/SRPMS
	mv rpm/packages/*.x86_64.rpm package/$(DISTRIBUTION)/x86_64

package.tar.gz:
	tar -chzf package.tar.gz package
