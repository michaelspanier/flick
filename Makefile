## Flick, built with Haiku's makefile-engine.
##
##   make                 build Flick into objects.*/
##   make bindcatalogs    put the translations into the program
##   make catkeys         write locales/en.catkeys from the sources
##   make -C tests        run the logic tests (any system with a C++ compiler)

NAME = Flick
TYPE = APP
APP_MIME_SIG = x-vnd.Flick

SRCS = \
	src/Flick.cpp \
	src/FlickCamera.cpp \
	src/FlickConsumer.cpp \
	src/FlickControls.cpp \
	src/FlickIcons.cpp \
	src/FlickLogic.cpp \
	src/FlickWindow.cpp

RDEFS = src/Flick.rdef

LIBS = be localestub media translation $(STDCPPLIBS)

LOCALES = de

OPTIMIZE := FULL
WARNINGS = ALL

## Include the makefile-engine
DEVEL_DIRECTORY := \
	$(shell findpaths -r "makefile_engine" B_FIND_PATH_DEVELOP_DIRECTORY)
include $(DEVEL_DIRECTORY)/etc/makefile-engine
