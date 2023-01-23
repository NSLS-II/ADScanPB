TOP=..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

SRC_DIRS = ../include

#add include files

INC +=

# Add libraries from vendor

ifeq (linux-x86_64, $(findstring linux-x86_64, $(T_A)))

#add libraries
LIB_INSTALLS_Linux +=

endif

ifeq (windows-x64, $(findstring windows-x64, $(T_A)))

#add libraries
LIB_INSTALLS_Windows +=

endif

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE


