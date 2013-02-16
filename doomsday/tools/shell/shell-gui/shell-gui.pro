# The Doomsday Engine Project: Server Shell -- GUI Interface
# Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
#
# This program is distributed under the GNU General Public License
# version 2 (or, at your option, any later version). Please visit
# http://www.gnu.org/licenses/gpl.html for details.

include(../../../config.pri)

TEMPLATE = app

      macx: TARGET = "Doomsday Shell"
else:win32: TARGET = Doomsday-Shell
      else: TARGET = doomsday-shell

VERSION = $$DENG_VERSION

# Build Configuration -------------------------------------------------------

DEFINES += SHELL_VERSION=\\\"$$VERSION\\\"

CONFIG += deng_qtgui

include(../../../dep_deng2.pri)
include(../../../dep_shell.pri)

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# Sources -------------------------------------------------------------------

HEADERS += \
    src/aboutdialog.h \
    src/folderselection.h \
    src/guishellapp.h \
    src/linkwindow.h \
    src/localserverdialog.h \
    src/opendialog.h \
    src/preferences.h \
    src/qtguiapp.h \
    src/qtrootwidget.h \
    src/qttextcanvas.h \
    src/statuswidget.h

SOURCES += \
    src/aboutdialog.cpp \
    src/folderselection.cpp \
    src/guishellapp.cpp \
    src/linkwindow.cpp \
    src/localserverdialog.cpp \
    src/main.cpp \
    src/opendialog.cpp \
    src/preferences.cpp \
    src/qtguiapp.cpp \
    src/qtrootwidget.cpp \
    src/qttextcanvas.cpp \
    src/statuswidget.cpp

RESOURCES += \
    res/shell.qrc

# Deployment ----------------------------------------------------------------

macx {
    ICON = res/macx/shell.icns
    QMAKE_INFO_PLIST = res/macx/Info.plist
    QMAKE_BUNDLE_DATA += res
    res.path = Contents/Resources
    res.files = res/macx/English.lproj

    # Clean up previous deployment.
    doPostLink("rm -rf \"Doomsday Shell.app/Contents/PlugIns/\"")
    doPostLink("rm -f \"Doomsday Shell.app/Contents/Resources/qt.conf\"")

    doPostLink("macdeployqt \"Doomsday Shell.app\"")
}
else {
    INSTALLS += target
    target.path = $$DENG_BIN_DIR
}
