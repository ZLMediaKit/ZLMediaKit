%if 0%{?fedora} >= 15 || 0%{?rhel} >= 7
%global use_devtoolset 0
%bcond_without faac
%bcond_without x264
%bcond_without webrtc
%else
%global use_devtoolset 1
%bcond_with faac
%bcond_with x264
%bcond_with webrtc
%endif

%bcond_without openssl
%bcond_without mysql

Name:		ZLMediaKit
Version:	5.0.0
Release:	1%{?dist}
Summary:	A lightweight, high performance and stable stream server and client framework based on C++11.

Group:		development
License:	MIT
URL:		https://github.com/xia-chu/ZLMediaKit
Source0:	%{name}-%{version}.tar.xz

%if %{with openssl}
%if 0%{?rhel} <= 7 && %{with webrtc}
BuildRequires:	openssl11-devel
%else
BuildRequires:	openssl-devel
%endif
%endif

%if %{with mysql}
BuildRequires:	mysql-devel
%endif

%if %{with faac}
BuildRequires:	faac-devel
%endif

%if %{with x264}
BuildRequires:	x264-devel
%endif

%if %{with webrtc}
BuildRequires:	libsrtp-devel >= 2.0
%endif

%if 0%{?use_devtoolset}
BuildRequires:	devtoolset-8-gcc-c++
%endif

%description
A lightweight RTSP/RTMP/HTTP/HLS/HTTP-FLV/WebSocket-FLV/HTTP-TS/HTTP-fMP4/WebSocket-TS/WebSocket-fMP4/GB28181 server and client framework based on C++11.

%package media-server
Requires:	%{name} = %{version}
Summary:	A lightweight, high performance and stable stream server

%description media-server
A lightweight RTSP/RTMP/HTTP/HLS/HTTP-FLV/WebSocket-FLV/HTTP-TS/HTTP-fMP4/WebSocket-TS/WebSocket-fMP4/GB28181 server.

%package c-libs
Requires:	%{name} = %{version}
Summary:	The %{name} C libraries.
%description c-libs
The %{name} C libraries.

%package c-devel
Requires:	%{name}-c-libs = %{version}
Summary:	The %{name} C API headers.
%description c-devel
The %{name} C API headers.

%package cxx-devel
Requires:	%{name} = %{version}
Summary:	The %{name} C++ API headers and development libraries.
%description cxx-devel
The %{name} C++ API headers and development libraries.

%prep
%setup -q

%build
mkdir -p %{_target_platform}

pushd %{_target_platform}

%if 0%{?use_devtoolset}
. /opt/rh/devtoolset-8/enable
%endif

%cmake3 \
    -DCMAKE_BUILD_TYPE:STRING=Release \
    -DENABLE_HLS:BOOL=ON \
    -DENABLE_OPENSSL:BOOL=%{with openssl} \
    -DENABLE_MYSQL:BOOL=%{with mysql} \
    -DENABLE_FAAC:BOOL=%{with faac} \
    -DENABLE_X264:BOOL=%{with x264} \
    -DENABLE_WEBRTC:BOOL=%{with webrtc} \
%if %{with webrtc} && 0%{?rhel} <= 7
    -DOPENSSL_ROOT_DIR:STRING="/usr/lib64/openssl11;/usr/include/openssl11" \
%endif
    -DENABLE_MP4:BOOL=ON \
    -DENABLE_RTPPROXY:BOOL=ON \
    -DENABLE_API:BOOL=ON \
    -DENABLE_CXX_API:BOOL=ON \
    -DENABLE_TESTS:BOOL=OFF \
    -DENABLE_SERVERL:BOOL=ON \
    ..

make %{?_smp_mflags}

popd

%install
rm -rf $RPM_BUILD_ROOT

pushd %{_target_platform}
%make_install
popd

install -m 0755 -d %{buildroot}%{_docdir}/%{name}
install -m 0644 LICENSE %{buildroot}%{_docdir}/%{name}
install -m 0644 README.md %{buildroot}%{_docdir}/%{name}
install -m 0644 README_en.md %{buildroot}%{_docdir}/%{name}

# TODO: service files

%clean
rm -rf $RPM_BUILD_ROOT

%files
%doc %{_docdir}/*

%files media-server
%{_bindir}/*

%files c-libs
%{_libdir}/libmk_api.so

%files c-devel
%{_includedir}/mk_*

%files cxx-devel
%{_includedir}/ZLMediaKit/*
%{_includedir}/ZLToolKit/*
%{_libdir}/libzlmediakit.a
%{_libdir}/libzltoolkit.a
%{_libdir}/libmpeg.a
%{_libdir}/libmov.a
%{_libdir}/libflv.a

%changelog

