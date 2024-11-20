#
# Unique file with all parameters that change in the IOC
#

# Maintainer
epicsEnvSet("ENGINEER",                 "Jakub Wlodek")

# IOC Information
epicsEnvSet("PORT",                     "PB1")
epicsEnvSet("IOC",                      "iocScanPB")

< /epics/common/localhost-netsetup.cmd

epicsEnvSet("EPICS_CA_MAX_ARRAY_BYTES", "6000000")


# PV and IOC Name configs
epicsEnvSet("PREFIX",                   "XF:31ID1-ES{Scan-PB:1}")
epicsEnvSet("HOSTNAME",                 "localhost")
epicsEnvSet("IOCNAME",                  "scanSim")
#epicsEnvSet("TRIGGER_SIGNAL",           "XF:31ID1-ES{PANDA:1}:LUT1:OUT")
epicsEnvSet("TRIGGER_SIGNAL",           "XF:31ID1-ES{SANDBOX:1}Clock")

# Imag and data size
epicsEnvSet("QSIZE",                    "30")
epicsEnvSet("NCHANS",                   "2048")
epicsEnvSet("HIST_SIZE",                "4096")
epicsEnvSet("XSIZE",                    "256")
epicsEnvSet("YSIZE",                    "256")
epicsEnvSet("NELMT",                    "65536")
epicsEnvSet("NDTYPE",                   "Int16")  #'Int8' (8bit B/W, Color) | 'Int16' (16bit B/W)
epicsEnvSet("NDFTVL",                   "SHORT") #'UCHAR' (8bit B/W, Color) | 'SHORT' (16bit B/W)
epicsEnvSet("CBUFFS",                   "500")

