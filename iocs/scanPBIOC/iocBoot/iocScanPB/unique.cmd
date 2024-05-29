#
# Unique file with all parameters that change in the IOC
#

# Maintainer
epicsEnvSet("ENGINEER",                 "Jakub Wlodek")

# IOC Information
epicsEnvSet("PORT",                     "PB1")
epicsEnvSet("IOC",                      "iocScanPB")

epicsEnvSet("EPICS_CA_AUTO_ADDR_LIST",  "NO")
epicsEnvSet("EPICS_CA_ADDR_LIST",       "127.0.0.255")
epicsEnvSet("EPICS_CAS_AUTO_BEACON_ADDR_LIST", "NO")           
epicsEnvSet("EPICS_CAS_BEACON_ADDR_LIST",      "127.0.0.255") 
epicsEnvSet("EPICS_CAS_INTF_ADDR_LIST",        "127.0.0.1") 


epicsEnvSet("EPICS_CA_MAX_ARRAY_BYTES", "6000000")


# PV and IOC Name configs
epicsEnvSet("PREFIX",                   "DEV:SCANPB:DET1:")
epicsEnvSet("HOSTNAME",                 "localhost")
epicsEnvSet("IOCNAME",                  "scanSim")

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

