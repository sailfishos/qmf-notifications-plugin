Name:       qmf-notifications-plugin
Summary:    Notifications plugin for Qt Messaging Framework (QMF)
Version:    0.1.1
Release:    1
License:    BSD
URL:        https://git.merproject.org/mer-core/qmf-notifications-plugin
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(QmfClient)
BuildRequires:  pkgconfig(QmfMessageServer)
BuildRequires:  pkgconfig(accounts-qt5) >= 1.13
BuildRequires:  pkgconfig(nemotransferengine-qt5)
BuildRequires:  pkgconfig(nemonotifications-qt5) >= 1.0.5
BuildRequires:  qt5-qttools-linguist

%description
%{summary}.

%prep
%setup -q -n %{name}-%{version}

%build

%qmake5 QMF_INSTALL_ROOT=/usr

make %{?_smp_mflags}

%install
rm -rf %{buildroot}
%qmake5_install

%files
%defattr(-,root,root,-)
%{_libdir}/qt5/plugins/messageserverplugins/libnotifications.so
%{_datadir}/translations/qmf-notifications_eng_en.qm

%package ts-devel
Summary:    Translation source for qmf-notifications-plugin
License:    BSD

%description ts-devel
%{summary}.

%files ts-devel
%defattr(-,root,root,-)
%{_datadir}/translations/source/qmf-notifications.ts
