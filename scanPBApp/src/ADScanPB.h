/*
 * Header file for the ADScanPB EPICS driver
 *
 * This file contains the definitions of PV params, and the definition of the ADScanPB class and
 * functions.
 *
 * This file was initially generated with the help of the ADDriverTemplate:
 * https://github.com/jwlodek/ADDriverTemplate on 23/01/2023
 *
 * Author: Jakub Wlodek
 *
 * Copyright (c) : Brookhaven National Laboratory, 2023
 *
 */

// header guard
#ifndef ADSCANPB_H
#define ADSCANPB_H

// version numbers
#define ADSCANPB_VERSION 0
#define ADSCANPB_REVISION 0
#define ADSCANPB_MODIFICATION 0

#define TRIG_TIMEOUT 5

typedef enum ADScanPBErr {
    ADSCANPB_LOG = 0,
    ADSCANPB_WARN = 1,
    ADSCANPB_ERR = 2,
} ADScanPBErr_t;

// Place PV string definitions here
#define ADScanPB_PlaybackRateFPSString "PLAYBACK_RATE_FPS"  //
#define ADScanPB_PlaybackRateSPFString "PLAYBACK_RATE_SPF"  //
#define ADScanPB_ScanFilePathString "SCAN_FILE_PATH"        //

#ifdef ADSCANPB_WITH_TILED_SUPPORT
#define ADScanPB_TiledMetadataURLString "TILED_METADATA_URL"
#endif

#define ADScanPB_TriggerEdgeString "TRIG_EDGE"
#define ADScanPB_TriggerSignalString "TRIG_SIGNAL"
#define ADScanPB_DataSourceString "DATA_SOURCE"
#define ADScanPB_ImageDatasetString "IMAGE_DATASET"  //
#define ADScanPB_ImageDatasetDescString "IMAGE_DATASET_DESC"  //
#define ADScanPB_TSDatasetString "TS_DATASET"        //
#define ADScanPB_AutoRepeatString "AUTO_REPEAT"      //
#define ADScanPB_ScanLoadedString "SCAN_LOADED"      //
#define ADScanPB_PlaybackPosString "PLAYBACK_POS"
#define ADScanPB_ResetPlaybackPosString "RESET_PLAYBACK_POS"
#define ADScanPB_NumFramesString "NUM_FRAMES"  //
#define ADScanPB_SupportedSourcesString "SUPPORTED_SOURCES"
#define ADScanPB_NumFramesLoadedString "NUM_FRAMES_LOADED"
#define ADScanPB_LoadPercentString "PERCENT_LOADED"

// Triggering records
#define ADScanPB_TriggerEdgeString "TRIG_EDGE"
#define ADScanPB_IdleReadySignalString "IDLE_READY_SIG"
#define ADScanPB_ReadySignalString "READY_SIGNAL"
#define ADScanPB_TriggerSignalString "TRIG_SIGNAL"
#define ADScanPB_NumTrigsRecdString "TRIGS_RECD"
#define ADScanPB_NumTrigsDroppedString "TRIGS_DROPPED"



// Place any required inclues here

#include "ADDriver.h"

#ifdef ADSCANPB_WITH_TILED_SUPPORT
#include <string>

#include "cpr/cpr.h"
#include "json.hpp"
using namespace std;

using json = nlohmann::json;
#endif

typedef enum {
    ADSCANPB_TRIG_INTERNAL = 0, // Purely software trigger
    ADSCANPB_TRIG_EDGE = 1, // Edge trigger, software exposure
    ADSCANPB_TRIG_EXP_GATE = 2, // Expose for trigger gate
    ADSCANPB_TRIG_ACQ_GATE = 3, // Acquire with internal clock during gate
} ADScanPBTrigMode_t;

typedef enum { ADSCANPB_EDGE_RISING = 0, ADSCANPB_EDGE_FALLING = 1 } ADScanPBTrigEdge_t;

typedef enum {
    ADSCANPB_DS_HDF5 = 1,
    ADSCANPB_DS_TIFF = 2,
    ADSCANPB_DS_JPEG = 4,
    ADSCANPB_DS_MP4 = 8,
    ADSCANPB_DS_TILED = 16,
    ADSCANPB_DS_KAFKA = 32,
} ADScanPBDataSource_t;

typedef enum {
    ADSCANPB_TIFF = 0,
    ADSCANPB_JPEG = 1,
} ADScanPBImageFormat_t;

// ----------------------------------------
// ScanPB Data Structures
//-----------------------------------------

// Place any in use Data structures here

/*
 * Class definition of the ADScanPB driver. It inherits from the base ADDriver class
 *
 * Includes constructor/destructor, PV params, function defs and variable defs
 *
 */
class ADScanPB : ADDriver {
   public:
    // Constructor
    ADScanPB(const char *portName, int maxBuffers, size_t maxMemory, int priority, int stackSize);

    // ADDriver overrides
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    virtual asynStatus writeOctet(asynUser *pasynUser, const char *value, size_t nChars,
                                  size_t *nActual);
    virtual asynStatus connect(asynUser *pasynUser);
    virtual asynStatus disconnect(asynUser *pasynUser);

    // destructor. Disconnects from camera, deletes the object
    ~ADScanPB();

    void playbackThread();

   protected:
    int ADScanPB_PlaybackRateFPS;
#define ADSCANPB_FIRST_PARAM ADScanPB_PlaybackRateFPS
    int ADScanPB_PlaybackRateSPF;
    int ADScanPB_ScanFilePath;

#ifdef ADSCANPB_WITH_TILED_SUPPORT
    int ADScanPB_TiledMetadataURL;
#endif

    int ADScanPB_TriggerSignal;
    int ADScanPB_TriggerEdge;
    int ADScanPB_DataSource;
    int ADScanPB_ImageDataset;
    int ADScanPB_ImageDatasetDesc;
    int ADScanPB_TSDataset;
    int ADScanPB_AutoRepeat;
    int ADScanPB_ScanLoaded;
    int ADScanPB_PlaybackPos;
    int ADScanPB_ResetPlaybackPos;
    int ADScanPB_NumFrames;
    int ADScanPB_SupportedSources;
    int ADScanPB_NumFramesLoaded;
    int ADScanPB_LoadPercent;
    int ADScanPB_IdleReadySignal;
    int ADScanPB_ReadySignal;
    int ADScanPB_NumTrigsRecd;
    int ADScanPB_NumTrigsDropped;
#define ADSCANPB_LAST_PARAM ADScanPB_NumTrigsDropped

   private:
    // Some data variables
    epicsEventId risingEdgeEventId;
    epicsEventId fallingEdgeEventId;
    bool waitingForTriggerEvent = false;

#ifdef ADSCANPB_WITH_TILED_SUPPORT
    string tiledApiKey;
    bool tiledConfigured = false;
#endif

    void *scanImageDataBuffer;
    void *scanTimestampDataBuffer;

    bool playback = false;

    epicsThreadId playbackThreadId;

    // ----------------------------------------
    // ScanPB Functions - Logging/Reporting
    //-----------------------------------------

    // reports device and driver info into a log file
    void report(FILE *fp, int details);

    // writes to ADStatus PV
    void updateStatus(const char *status, ADScanPBErr_t errLevel);

    void updateImageDatasetDesc(ADScanPBDataSource_t dataSource);

    // ----------------------------------------
    // ScanPB Functions - Scan Load Functions
    //-----------------------------------------

    asynStatus openScanHDF5(const char *filePath);
    // asynStatus openScanImageSeries(const char *filePath, ADScanPBImageFormat_t format);
    // asynStatus openScanMP4(const char *filePath);

#ifdef ADSCANPB_WITH_TILED_SUPPORT
    asynStatus openScanTiled(const char *nodePath);
#endif

    void closeScan();

    void setPlaybackRate(int rateFormat);

    // function that begins image aquisition
    asynStatus acquireStart();

    // function that stops aquisition
    asynStatus acquireStop();
};

// Stores number of additional PV parameters are added by the driver
#define NUM_SCANPB_PARAMS ((int)(&ADSCANPB_LAST_PARAM - &ADSCANPB_FIRST_PARAM + 1))

#endif
