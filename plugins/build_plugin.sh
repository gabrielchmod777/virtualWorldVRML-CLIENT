#!/bin/bash
#
# Coin3D and SoQt related
#
CPPFLAGS_COIN=`soqt-config --cppflags`
CXXFLAGS_COIN=`soqt-config --cxxflags`
LDFLAGS_COIN=`soqt-config --ldflags`
LIBS_COIN=`soqt-config --libs`

CPPFLAGS_QT=`soqt-config --cppflags`
CXXFLAGS_QT=`soqt-config --cxxflags`
LDFLAGS_QT=`soqt-config --ldflags`
LIBS_QT=`soqt-config --libs`
INCLUDEDIR_QT=" -I/usr/include/qt4 -I/usr/include/qt4/QtCore -I/usr/include/qt4/QtGui -I/usr/include/qt4/QtOpenGL  -I/usr/include/qt4/QtWebKit "
#`soqt-config --includedir`

#
# [ -Wall ] display all warnings
# [ -fPIC ] generate Position Independent Code
#
#  MODEL g++ -c -Wall -fPIC plugin1.cpp  -o plugin1.o

g++ -c -Wall -fPIC $INCLUDEDIR_QT $1 -o $2.o 

#
# [ -Wl,-snoname,<name> ] instuct the linker the
# linker to generate a Dynamic Shared Objects
# instead of an application
#
#g++             -shared -Wl,-soname,cos.so -o cos.so plugin1.o

g++ $CPPFLAGS_COIN $CXXFLAGS_COIN $CPPFLAGS_QT $CXXFLAGS_QT -I$INCLUDEDIR_QT -shared -Wl,-soname,$3.so -o $3.so $2.o $LDFLAGS_COIN $LIBS_COIN  $LDFLAGS_QT $LIBS_QT
# create links
#ln -sf $3.so.1.0 $3.so.1
#ln -sf $3.so.1.0 $3.so
