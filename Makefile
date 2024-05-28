#Makefile at top of application tree
TOP = .
include $(TOP)/configure/CONFIG
DIRS := $(DIRS) configure
DIRS := $(DIRS) scanPBApp

# To allow for playback from tiled sources, 
ifeq ($(WITH_TILED_SUPPORT), YES)
DIRS := $(DIRS) scanPBSupport
scanPBApp_DEPEND_DIRS += scanPBSupport
endif

ifeq ($(BUILD_IOCS), YES)
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard iocs))
iocs_DEPEND_DIRS += scanPBApp
endif
include $(TOP)/configure/RULES_TOP

uninstall: uninstall_iocs
uninstall_iocs:
	$(MAKE) -C iocs uninstall
.PHONY: uninstall uninstall_iocs

realuninstall: realuninstall_iocs
realuninstall_iocs:
	$(MAKE) -C iocs realuninstall
.PHONY: realuninstall realuninstall_iocs
