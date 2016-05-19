%global version {{version}}
%define debug_package %{nil}

Name:		aws-sdk-cpp
Version:	%{version}
Release:	{{build}}%{?dist}
Summary:	AWS SDK for C++
Group:		Applications/File
License:	MIT
URL:		https://github.com/aws/aws-sdk-cpp
Source0:	aws-sdk-cpp-%{version}.orig.tar.gz

BuildRequires: cmake >= 3.0.0,
BuildRequires: gcc-c++ >= 4.9.0,
BuildRequires: git,
BuildRequires: libcurl-devel,
BuildRequires: openssl-devel

%description
AWS SDK for C++

%prep
%setup -q

%build
%cmake . -DCMAKE_BUILD_TYPE=Release -DBUILD_ONLY=s3 -DSTATIC_LINKING=1 -DCUSTOM_MEMORY_MANAGEMENT=0
make %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot}
find %{buildroot}

%files
%{_prefix}/lib/*
%{_includedir}/*

%doc

%changelog

