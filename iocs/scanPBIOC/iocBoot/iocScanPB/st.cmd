#!../../bin/linux-x86_64/scanPBApp


< unique.cmd
errlogInit(20000)

< envPaths

epicsEnvSet("EPICS_DB_INCLUDE_PATH", "$(ADCORE)/db")

#epicsThreadSleep(20)
dbLoadDatabase("$(ADSCANPB)/iocs/scanPBIOC/dbd/scanPBApp.dbd")
scanPBApp_registerRecordDeviceDriver(pdbbase) 


#/*
# * Constructor for ADScanPB driver. Most params are passed to the parent ADDriver constructor. 
# * Connects to the camera, then gets device information, and is ready to aquire images.
# *
# * NOTE THAT THIS IS AUTOGENERATED BY THE TEMPLATE, IT IS POSSIBLE THAT YOUR DETECTOR DOES NOT USE 
# * SERIAL NUMBER TO CONNECT, AND THIS MUST BE CHANGED
# * 
# * @params: portName -> port for NDArray recieved from camera
# * @params: serial -> serial number of device to connect to
# * @params: maxBuffers -> max buffer size for NDArrays
# * @params: maxMemory -> maximum memory allocated for driver
# * @params: priority -> what thread priority this driver will execute with
# * @params: stackSize -> size of the driver on the stack
# */


#
# ADScanPBConfig(const char* portName, const char* serial, int maxBuffers, size_t maxMemory, int priority, int stackSize)
# epicsThreadSleep(2)

# epicsEnvSet("TILED_METADATA_URL", "https://tiled.nsls2.bnl.gov/api/v1/node/metadata")
# epicsEnvSet("TILED_ARRAY_URL", "https://tiled.nsls2.bnl.gov/api/v1/array/full")

# If searching for device by product ID put "" or empty string for serial number
ADScanPBConfig("$(PORT)", 0, 0, 0, 0)
epicsThreadSleep(2)

asynSetTraceIOMask($(PORT), 0, 2)
#asynSetTraceMask($(PORT),0,0xff)

dbLoadRecords("$(ADCORE)/db/ADBase.template", "P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADSCANPB)/db/ADScanPB.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADSCANPB)/db/ADScanPBTrig.template","P=$(PREFIX),R=cam1:,TRIGGER_SIGNAL=$(TRIGGER_SIGNAL),PORT=$(PORT),ADDR=0,TIMEOUT=1")

# Optionally, if tiled support is included
#dbLoadRecords("$(ADSCANPB)/db/ADScanPBTiled.template", "P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1, TILED_METADATA_URL=$(TILED_METADATA_URL), TILED_ARRAY_URL=$(TILED_ARRAY_URL)")

#
# Create a standard arrays plugin, set it to get data from Driver.
#int NDStdArraysConfigure(const char *portName, int queueSize, int blockingCallbacks, const char *NDArrayPort, int NDArrayAddr, int maxBuffers, size_t maxMemory,
#                          int priority, int stackSize, int maxThreads)
NDStdArraysConfigure("Image1", 3, 0, "$(PORT)", 0)
#dbLoadRecords("$(ADCORE)/db/NDPluginBase.template","P=$(PREFIX),R=image1:,PORT=Image1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT),NDARRAY_ADDR=0")
#dbLoadRecords("$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX),R=image1:,PORT=Image1,ADDR=0,TIMEOUT=1,TYPE=Int16,SIZE=16,FTVL=SHORT,NELEMENTS=802896")
dbLoadRecords("$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX),R=image1:,PORT=Image1,ADDR=0,NDARRAY_PORT=$(PORT),TIMEOUT=1,TYPE=Int16,FTVL=SHORT,NELEMENTS=20000000")

#
# Load all other plugins using commonPlugins.cmd
< $(ADCORE)/iocBoot/commonPlugins.cmd
#

set_requestfile_path("$(ADSCANPB)/scanPBApp/Db")

#asynSetTraceMask($(PORT),0,0x09)
#asynSetTraceMask($(PORT),0,0x11)
iocInit()

# save things every thirty seconds
create_monitor_set("auto_settings.req", 30, "P=$(PREFIX)")

# Load scan at startup
dbpf $(PREFIX)cam1:ImageDataset "/entry/data/data"
dbpf $(PREFIX)cam1:ScanPath "/nsls2/data/tst/legacy/mock-proposals/2024-1/pass-000000/01bf70d7-fec9-4336-9c9e-a971cb1cf286_000.h5"
