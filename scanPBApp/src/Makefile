TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

ifeq (windows-x64, $(findstring windows-x64, $(T_A)))
USR_CPPFLAGS += -D__X64
endif

USR_CPPFLAGS += -std=c++11

ifeq ($(WITH_TILED_SUPPORT), YES)
USR_CPPFLAGS += -DADSCANPB_WITH_TILED_SUPPORT
endif

INC += ADScanPB.h

LIBRARY_IOC = ADScanPB 
LIB_SRCS += ADScanPB.cpp

ifeq ($(WITH_TILED_SUPPORT), YES)
LIB_SYS_LIBS += cpr curl z
endif

DBD += scanPBSupport.dbd

include $(ADCORE)/ADApp/commonLibraryMakefile

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
