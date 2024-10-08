TEMPLATE = lib
TARGET = notifications
CONFIG += plugin hide_symbols

QT += qmfclient qmfmessageserver
QT -= gui

CONFIG += link_pkgconfig
PKGCONFIG += nemotransferengine-qt5 nemonotifications-qt5 nemoemail-qt5

SOURCES += \
    actionobserver.cpp \
    notificationsplugin.cpp \
    mailstoreobserver.cpp

HEADERS += \
    actionobserver.h \
    notificationsplugin.h \
    mailstoreobserver.h

OTHER_FILES += \
    rpm/qmf-notifications-plugin.spec

# translations
TS_FILE = $$OUT_PWD/qmf-notifications.ts
EE_QM = $$OUT_PWD/qmf-notifications_eng_en.qm
ts.commands += lupdate $$PWD -ts $$TS_FILE
ts.CONFIG += no_check_exist
ts.output = $$TS_FILE
ts.input = .
ts_install.files = $$TS_FILE
ts_install.path = /usr/share/translations/source
ts_install.CONFIG += no_check_exist

# should add -markuntranslated "-" when proper translations are in place (or for testing)
engineering_english.commands += lrelease -idbased $$TS_FILE -qm $$EE_QM
engineering_english.CONFIG += no_check_exist
engineering_english.depends = ts
engineering_english.input = $$TS_FILE
engineering_english.output = $$EE_QM
engineering_english_install.path = /usr/share/translations
engineering_english_install.files = $$EE_QM
engineering_english_install.CONFIG += no_check_exist

QMAKE_EXTRA_TARGETS += ts engineering_english
PRE_TARGETDEPS += ts engineering_english

target.path = $$[QT_INSTALL_PLUGINS]/messageserverplugins

INSTALLS += target ts_install engineering_english_install
