TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

ifeq (windows-x64, $(findstring windows-x64, $(T_A)))
USR_CPPFLAGS += -D__X64
endif

USR_CPPFLAGS += -std=c++11

USR_CPPFLAGS += -DADSCANPB_WITH_TILED_SUPPORT

INC += ADScanPB.h

LIBRARY_IOC = ADScanPB 
LIB_SRCS += ADScanPB.cpp

LIB_SYS_LIBS += cpr curl z

DBD += scanPBSupport.dbd

include $(ADCORE)/ADApp/commonLibraryMakefile

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
