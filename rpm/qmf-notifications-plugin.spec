Name:       qmf-notifications-plugin
Summary:    Notifications plugin for Qt Messaging Framework (QMF)
Version:    0.2.7
Release:    1
License:    BSD
URL:        https://github.com/sailfishos/qmf-notifications-plugin
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(QmfClient)
BuildRequires:  pkgconfig(QmfMessageServer)
BuildRequires:  pkgconfig(nemotransferengine-qt5)
BuildRequires:  pkgconfig(nemonotifications-qt5) >= 1.2.6
BuildRequires:  pkgconfig(nemoemail-qt5)
BuildRequires:  qt5-qttools-linguist

%description
%{summary}.

%package ts-devel
Summary:    Translation source for qmf-notifications-plugin
License:    BSD

%description ts-devel
%{summary}.

%prep
%setup -q -n %{name}-%{version}

%build

%qmake5 QMF_INSTALL_ROOT=/usr
%make_build

%install
%qmake5_install

%files
%license LICENSE.BSD
%{_libdir}/qt5/plugins/messageserverplugins/libnotifications.so
%{_datadir}/translations/qmf-notifications_eng_en.qm

%files ts-devel
%{_datadir}/translations/source/qmf-notifications.ts
