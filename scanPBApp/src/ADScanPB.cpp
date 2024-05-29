/**
 * Main source file for the ADScanPB EPICS driver
 *
 * This file was initially generated with the help of the ADDriverTemplate:
 * https://github.com/jwlodek/ADDriverTemplate on 23/01/2023
 *
 * This file contains functions for connecting and disconnectiong from the camera,
 * for starting and stopping image acquisition, and for controlling all camera functions through
 * EPICS.
 *
 * Author: Jakub Wlodek
 *
 * Copyright (c) : Brookhaven National Laboratory, 2023
 *
 */

// Standard includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// EPICS includes
#include <epicsExit.h>
#include <epicsExport.h>
#include <epicsStdio.h>
#include <epicsString.h>
#include <epicsThread.h>
#include <epicsTime.h>
#include <iocsh.h>

#define H5Gcreate_vers 2
#define H5Dopen_vers 2

#include <hdf5.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <cmath>
#include <iostream>
#include <sstream>

// Area Detector include
#include "ADScanPB.h"

// Error message formatters
#define ERR(msg) \
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "ERR  | %s::%s: %s\n", driverName, functionName, msg)

#define ERR_ARGS(fmt, ...)                                                             \
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "ERR  | %s::%s: " fmt "\n", driverName, \
              functionName, __VA_ARGS__);

// Warning message formatters
#define WARN(msg) \
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "WARN | %s::%s: %s\n", driverName, functionName, msg)

#define WARN_ARGS(fmt, ...)                                                            \
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "WARN | %s::%s: " fmt "\n", driverName, \
              functionName, __VA_ARGS__);

// Log message formatters
#define LOG(msg) \
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "LOG  | %s::%s: %s\n", driverName, functionName, msg)

#define LOG_ARGS(fmt, ...)                                                             \
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "LOG  | %s::%s: " fmt "\n", driverName, \
              functionName, __VA_ARGS__);

using namespace std;

// Add any additional namespaces here

const char *driverName = "ADScanPB";

// Add any driver constants here

// -----------------------------------------------------------------------
// ADScanPB Utility Functions (Reporting/Logging/ExternalC)
// -----------------------------------------------------------------------

/*
 * External configuration function for ADScanPB.
 * Envokes the constructor to create a new ADScanPB object
 * This is the function that initializes the driver, and is called in the IOC startup script
 *
 * @params[in]: all passed into constructor
 * @return:     status
 */
extern "C" int ADScanPBConfig(const char *portName, int maxBuffers, size_t maxMemory, int priority,
                               int stackSize) {
    new ADScanPB(portName, maxBuffers, maxMemory, priority, stackSize);
    return (asynSuccess);
}

/*
 * Callback function called when IOC is terminated.
 * Deletes created object
 *
 * @params[in]: pPvt -> pointer to the ADScanPB object created in ADScanPBConfig
 * @return:     void
 */
static void exitCallbackC(void *pPvt) {
    ADScanPB *pScanPB = (ADScanPB *)pPvt;
    delete (pScanPB);
}

static void playbackThreadC(void *pPvt) {
    ADScanPB *pScanPB = (ADScanPB *)pPvt;
    pScanPB->playbackThread();
}

void ADScanPB::updateStatus(const char *msg, ADScanPBErr_t errLevel) {
    const char *functionName = "updateStatus";
    switch (errLevel) {
        case ADSCANPB_LOG:
            LOG(msg);
            break;
        case ADSCANPB_WARN:
            WARN(msg);
            break;
        default:
            ERR(msg);
            break;
    }
    setStringParam(ADStatusMessage, msg);
}

// -----------------------------------------------------------------------
// ADScanPB Acquisition Functions
// -----------------------------------------------------------------------

/**
 * Function responsible for starting camera image acqusition. First, check if there is a
 * camera connected. Then, set camera values by reading from PVs. Then, we execute the
 * Acquire Start command. if this command was successful, image acquisition started.
 *
 * @return: status  -> error if no device, camera values not set, or execute command fails.
 * Otherwise, success
 */
asynStatus ADScanPB::acquireStart() {
    const char *functionName = "acquireStart";
    asynStatus status = asynSuccess;

    int scanLoaded;
    getIntegerParam(ADScanPB_ScanLoaded, &scanLoaded);
    if (scanLoaded != 1) {
        updateStatus("Scan has not been loaded for playback!", ADSCANPB_ERR);
        status = asynError;
    }

    if (status != asynSuccess) {
        setIntegerParam(ADAcquire, 0);
        setIntegerParam(ADStatus, ADStatusIdle);
    } else {
        setIntegerParam(ADStatus, ADStatusAcquire);
        LOG("Image acquistion start");

        epicsThreadOpts opts;
        opts.priority = epicsThreadPriorityMedium;
        opts.stackSize = epicsThreadGetStackSize(epicsThreadStackMedium);
        opts.joinable = 1;
        this->playback = true;

        this->playbackThreadId =
            epicsThreadCreateOpt("playbackThread", (EPICSTHREADFUNC)playbackThreadC, this, &opts);
    }

    return status;
}

void ADScanPB::playbackThread() {
    const char *functionName = "playbackThread";

    NDArray *pArray;
    NDArrayInfo arrayInfo;
    int dataType, imageMode, colorMode, ndims, autoRepeat, nframes, arrayCallbacks, trigSignal;
    ADScanPBTrigMode_t trigMode;
    ADScanPBTrigEdge_t trigEdge;
    ADScanPBTTLSignal_t idleSignal, busySignal;
    getIntegerParam(ADScanPB_IdleReadySignal, (int*) &idleSignal);
    if (idleSignal == ADSCANPB_SIGNAL_HIGH)
        busySignal = ADSCANPB_SIGNAL_LOW;
    else
        busySignal = ADSCANPB_SIGNAL_HIGH;

    clock_t start, end;
    double playbackTime;

    // Three different frame counters.
    int playbackPos, imageCounter, totalImageCounter;

    getIntegerParam(NDColorMode, &colorMode);
    getIntegerParam(NDDataType, &dataType);

    getIntegerParam(ADImageMode, &imageMode);
    getIntegerParam(ADScanPB_NumFrames, &nframes);

    getIntegerParam(ADTriggerMode, (int *)&trigMode);
    getIntegerParam(ADScanPB_TriggerEdge, (int *)&trigEdge);

    int width, height;
    getIntegerParam(ADMaxSizeX, &width);
    getIntegerParam(ADMaxSizeY, &height);

    if ((NDColorMode_t)colorMode == NDColorModeMono)
        ndims = 2;
    else
        ndims = 3;

    size_t dims[ndims];
    if (ndims == 2) {
        dims[0] = width;
        dims[1] = height;
    } else {
        dims[0] = 3;
        dims[1] = width;
        dims[2] = height;
    }

    bool acqStarted = false;

    while (playback) {
        start = clock();
        int lastSignal;
        getIntegerParam(ADScanPB_TriggerSignal, &trigSignal);
        lastSignal = trigSignal;
        setIntegerParam(ADScanPB_ReadySignal, (int) idleSignal);
        callParamCallbacks();

        if (trigMode != ADSCANPB_TRIG_INTERNAL) {
            if (trigMode != ADSCANPB_TRIG_ACQ_GATE || !acqStarted) {
                updateStatus("Armed, waiting for trigger.", ADSCANPB_LOG);
                setIntegerParam(ADStatus, ADStatusWaiting);
                callParamCallbacks();
                // // Wait for change in trigger signal, if goes from low to high, rising edge, otherwise falling edge.
                // while (!(lastSignal != trigSignal && trigSignal != (int) trigEdge)) {
                //     if(!playback) break;
                //     getIntegerParam(ADScanPB_TriggerSignal, &trigSignal);
                //     if(trigSignal == trigEdge) lastSignal = trigSignal;
                // }
                // if(!playback) break; // if we are exiting loop because of abort, break outer loop
                epicsEventWaitStatus eventRecd = epicsEventWaitError;
                while (eventRecd != epicsEventWaitOK) {
                    if (trigEdge == ADSCANPB_EDGE_RISING)
                        eventRecd = epicsEventWaitWithTimeout(this->risingEdgeEventId, TRIG_TIMEOUT);
                    else
                        eventRecd = epicsEventWaitWithTimeout(this->fallingEdgeEventId, TRIG_TIMEOUT);
                    if(!playback) break;
                }

                acqStarted = true;
                this->waitingForTriggerEvent = false;
                LOG_ARGS("Recieved %s edge trigger.",
                         trigEdge == ADSCANPB_EDGE_RISING ? "rising" : "falling");
            }
        }
        setIntegerParam(ADScanPB_ReadySignal, (int) busySignal);

        double spf;
        getIntegerParam(ADScanPB_AutoRepeat, &autoRepeat);
        getDoubleParam(ADScanPB_PlaybackRateSPF, &spf);
        getIntegerParam(ADScanPB_PlaybackPos, &playbackPos);
        LOG_ARGS("Playing back frame %d from scan...", playbackPos);

        // allocate memory for a new NDArray, and set pArray to a pointer for this memory
        this->pArrays[0] = pNDArrayPool->alloc(ndims, dims, (NDDataType_t)dataType, 0, NULL);

        if (this->pArrays[0] != NULL) {
            pArray = this->pArrays[0];
        } else {
            this->pArrays[0]->release();
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Unable to allocate array\n",
                      driverName, functionName);
            return;
        }

        updateTimeStamp(&pArray->epicsTS);

        size_t num_elems = 1;
        for (int i = 0; i < ndims; i++) {
            num_elems = num_elems * dims[i];
        }

        size_t totalBytes = num_elems;
        if (dataType == NDUInt8) {
            memcpy(pArray->pData,
                   (uint8_t *)this->scanImageDataBuffer + (num_elems * (playbackPos)), num_elems);
        } else {
            totalBytes = totalBytes * 2;
            memcpy(pArray->pData,
                   (uint16_t *)this->scanImageDataBuffer + (num_elems * (playbackPos)), totalBytes);
        }

        pArray->pAttributeList->add("ColorMode", "Color Mode", NDAttrInt32, &colorMode);

        getIntegerParam(NDArrayCounter, &imageCounter);
        imageCounter++;
        setIntegerParam(NDArrayCounter, imageCounter);

        getIntegerParam(ADNumImagesCounter, &totalImageCounter);
        totalImageCounter++;
        setIntegerParam(ADNumImagesCounter, totalImageCounter);
        pArray->uniqueId = totalImageCounter;

        setIntegerParam(NDArraySizeX, width);
        setIntegerParam(NDArraySizeY, height);
        setIntegerParam(NDArraySize, totalBytes);

        // If we don't have a timestamp buffer loaded, create new timestamp
        if (this->scanTimestampDataBuffer == NULL) {
            pArray->timeStamp =
                (double)pArray->epicsTS.secPastEpoch + ((double)pArray->epicsTS.nsec * 1.0e-9);
        } else {
            pArray->timeStamp = *((double *)this->scanTimestampDataBuffer + playbackPos);
        }

        end = clock();
        playbackTime = ((double)(end - start)) / CLOCKS_PER_SEC;

        // Unless we are in gated exposure mode, wait for the desired exposure time.
        if(trigMode != ADSCANPB_TRIG_EXP_GATE) {
            epicsThreadSleep(spf - playbackTime);
        }

        getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
        if (arrayCallbacks) doCallbacksGenericPointer(pArray, NDArrayData, 0);

        pArray->release();

        playbackPos++;

        if (imageMode == ADImageSingle) {
            playback = false;
        }

        else if (imageMode == ADImageMultiple) {
            int desiredImages;
            getIntegerParam(ADNumImages, &desiredImages);
            if (desiredImages <= imageCounter) playback = false;
        }

        if (playbackPos == nframes) {
            playbackPos = 0;
            if (autoRepeat != 1) playback = false;
        }

        setIntegerParam(ADScanPB_PlaybackPos, playbackPos);

        if (!playback) {
            setIntegerParam(ADAcquire, 0);
            setIntegerParam(ADStatus, ADStatusIdle);
        }
        callParamCallbacks();
    }
}

/**
 * Function responsible for stopping camera image acquisition. First check if the camera is
 * connected. If it is, execute the 'AcquireStop' command. Then set the appropriate PV values, and
 * callParamCallbacks
 *
 * @return: status  -> error if no camera or command fails to execute, success otherwise
 */
asynStatus ADScanPB::acquireStop() {
    const char *functionName = "acquireStop";
    asynStatus status = asynSuccess;

    // Stop acquistion and join back the playback thread
    if (this->playback) {
        this->playback = false;
        epicsThreadMustJoin(this->playbackThreadId);
    }

    setIntegerParam(ADStatus, ADStatusIdle);
    LOG("Stopping Image Acquisition");
    callParamCallbacks();
    return status;
}

// Here, you will need three functions:
// * A function for converting the images supplied by vendor software into areaDetector NDArrays
// * A function that converts the data type and color mode of the vendor software supplied image
// structure into areaDetector NDDataType_t and NDColorMode_t
// * A function for that takes a pointer to a vendor software image data structure (your callback
// function - runs once per image)
//
// If the vendor software is expecting a static function pointer as the callback parameter, you can
// create a static function with a void pointer as an argument cast that void pointer to type
// ADScanPB*, and call the callback function

//---------------------------------------------------------
// Base ScanPB Camera functionality
//---------------------------------------------------------

//  Add functions for getting/setting various camera settings (gain, exposure etc.) here

asynStatus ADScanPB::connect(asynUser *pasynUser) { return asynSuccess; }

asynStatus ADScanPB::disconnect(asynUser *pasynUser) {
    const char *functionName = "disconnect";
    asynStatus status = pasynManager->exceptionDisconnect(this->pasynUserSelf);
    if (status) {
        ERR_ARGS("error calling pasynManager->exceptionDisconnect, error=%s",
                 pasynUserSelf->errorMessage);
    }
    return status;
}

//-------------------------------------------------------------------------
// ADDriver function overwrites
//-------------------------------------------------------------------------

/*
 * Function overwriting ADDriver base function.
 * Takes in a function (PV) changes, and a value it is changing to, and processes the input
 *
 * @params[in]: pasynUser       -> asyn client who requests a write
 * @params[in]: value           -> int32 value to write
 * @return: asynStatus      -> success if write was successful, else failure
 */
asynStatus ADScanPB::writeInt32(asynUser *pasynUser, epicsInt32 value) {
    int function = pasynUser->reason;
    int acquiring;
    asynStatus status = asynSuccess;
    static const char *functionName = "writeInt32";
    getIntegerParam(ADAcquire, &acquiring);

    status = setIntegerParam(function, value);
    // start/stop acquisition
    if (function == ADAcquire) {
        if (value && !acquiring) {
            status = acquireStart();
            if (status == asynError) {
                updateStatus("Failed to start acquisition", ADSCANPB_ERR);
                status = asynError;
            }
        }
        if (!value && acquiring) {
            status = acquireStop();
        }
    } else if (function == ADScanPB_ResetPlaybackPos) {
        setIntegerParam(ADScanPB_PlaybackPos, 0);
    } else if (function == ADImageMode) {
        if (acquiring == 1) acquireStop();
    } else if (function == NDDataType || function == NDColorMode) {
        updateStatus("Color mode and data type are read from loaded scan", ADSCANPB_ERR);
        status = asynError;
    } else if (function == ADStatus) {
        if (value == ADStatusIdle) printf("SAW STAT TO IDLE");
    } else if (function == ADScanPB_DataSource) {
        updateImageDatasetDesc((ADScanPBDataSource_t) value);
    } else if (function == ADScanPB_TriggerSignal){
        ADScanPBTrigEdge_t trigEdge;
        getIntegerParam(ADScanPB_TriggerEdge, (int*) trigEdge);

        epicsEventId edgeEvent = NULL;
        if (value == 1) {
            edgeEvent = this->risingEdgeEventId;
        } else if (value == 0) {
            edgeEvent = this->fallingEdgeEventId;
        }
        if (edgeEvent != NULL){
            int numTriggersRecd, numTriggersDropped;
            ADScanPBTTLSignal_t idleSignal, readySignal;
            
            getIntegerParam(ADScanPB_IdleReadySignal, (int*) &idleSignal);
            getIntegerParam(ADScanPB_ReadySignal, (int*) &readySignal);
            getIntegerParam(ADScanPB_NumTrigsRecd, &numTriggersRecd);
            getIntegerParam(ADScanPB_NumTrigsDropped, &numTriggersDropped);
            
            numTriggersRecd += 1;
            if (readySignal != idleSignal){
                numTriggersDropped += 1;
            } else{
                epicsEventSignal(edgeEvent);
            }
            setIntegerParam(ADScanPB_NumTrigsRecd, (int)numTriggersRecd);
            setIntegerParam(ADScanPB_NumTrigsDropped, (int)numTriggersDropped);
        }
        
    } else {
        if (function < ADSCANPB_FIRST_PARAM) {
            status = ADDriver::writeInt32(pasynUser, value);
        }
    }
    callParamCallbacks();

    if (status) {
        ERR_ARGS("status=%d, function=%d, value=%d", status, function, value);
        return asynError;
    } else
        LOG_ARGS("function=%d value=%d", function, value);
    return status;
}

void ADScanPB::setPlaybackRate(int rateFormat) {
    const char *functionName = "setPlaybackRate";
    double fps, spf;

    if (rateFormat == ADScanPB_PlaybackRateFPS) {
        getDoubleParam(rateFormat, &fps);
        spf = 1 / fps;
        setDoubleParam(ADScanPB_PlaybackRateSPF, spf);
    } else {
        getDoubleParam(rateFormat, &spf);
        fps = 1 / spf;
        setDoubleParam(ADScanPB_PlaybackRateFPS, fps);
    }
    LOG_ARGS("User set playback FPS to %lf, or %lf seconds per frame.", fps, spf);
}

/*
 * Function overwriting ADDriver base function.
 * Takes in a function (PV) changes, and a value it is changing to, and processes the input
 * This is the same functionality as writeInt32, but for processing doubles.
 *
 * @params[in]: pasynUser       -> asyn client who requests a write
 * @params[in]: value           -> int32 value to write
 * @return: asynStatus      -> success if write was successful, else failure
 */
asynStatus ADScanPB::writeFloat64(asynUser *pasynUser, epicsFloat64 value) {
    int function = pasynUser->reason;
    int acquiring;
    asynStatus status = asynSuccess;
    static const char *functionName = "writeFloat64";
    getIntegerParam(ADAcquire, &acquiring);

    status = setDoubleParam(function, value);

    if (function == ADScanPB_PlaybackRateFPS || function == ADScanPB_PlaybackRateSPF) {
        setPlaybackRate(function);
    } else {
        if (function < ADSCANPB_FIRST_PARAM) {
            status = ADDriver::writeFloat64(pasynUser, value);
        }
    }
    callParamCallbacks();

    if (status) {
        ERR_ARGS("status = %d, function =%d, value = %f", status, function, value);
        return asynError;
    } else
        LOG_ARGS("function=%d value=%f", function, value);
    return status;
}

void ADScanPB::closeScan() {
    // If acquiring, stop acquiring first.
    if (this->playback) acquireStop();

    // clear out buffers if they have been allocated
    if (this->scanImageDataBuffer != NULL) free(this->scanImageDataBuffer);

    if (this->scanTimestampDataBuffer != NULL) free(this->scanTimestampDataBuffer);

    setIntegerParam(ADScanPB_ScanLoaded, 0);
    setDoubleParam(ADScanPB_LoadPercent, 0);
    setIntegerParam(ADScanPB_NumFramesLoaded, 0);
    callParamCallbacks();
}

#ifdef ADSCANPB_WITH_TILED_SUPPORT
asynStatus ADScanPB::openScanTiled(const char *nodePath) {
    const char *functionName = "openScanTiled";
    asynStatus status = asynSuccess;

    char metadataURL[256];
    getStringParam(ADScanPB_ImageDataset, 256, metadataURL);
    printf("%s\n", metadataURL);

    if (!this->tiledApiKey.empty() && strlen(metadataURL) != 0) this->tiledConfigured = true;

    if (!this->tiledConfigured) updateStatus("Tiled configuration incomplete!", ADSCANPB_WARN);

    LOG_ARGS("Attempting to load scan from Tiled node: %s", nodePath);

    cpr::Response r;
    if (this->tiledApiKey.empty()) {
        r = cpr::Get(cpr::Url{string(nodePath)});
    } else {
        cpr::Header auth = cpr::Header{{string("Authorization"), "Apikey " + this->tiledApiKey}};
        r = cpr::Get(cpr::Url{string(nodePath)}, auth);
    }

    if (r.status_code != 200) {
        updateStatus(r.text.c_str(), ADSCANPB_ERR);
        return asynError;
    }

    cout << r.text << endl;

    json metadata_j = json::parse(r.text.c_str());
    json scanShape = metadata_j["data"]["attributes"]["structure"]["macro"]["shape"];
    int numAcquistions = scanShape[0].get<int>();
    int numFrames = scanShape[1].get<int>() * numAcquistions;
    int ySize = scanShape[2].get<int>();
    int xSize = scanShape[3].get<int>();
    int bytesPerElem =
        metadata_j["data"]["attributes"]["structure"]["micro"]["itemsize"].get<int>();
    json chunks = metadata_j["data"]["attributes"]["structure"]["macro"]["chunks"];

    string dataURL = metadata_j["data"]["links"]["block"];
    char *dataURLToken = strtok((char *)dataURL.c_str(), "?");
    dataURL = string(dataURLToken);
    cout << dataURL << endl;

    // determine whether or not the image data is in color or not.
    // if(colorChannels == 3){
    //    setIntegerParam(NDColorMode, NDColorModeRGB1);
    //} else {
    setIntegerParam(NDColorMode, NDColorModeMono);
    //}

    updateStatus("Loading scan from URL...", ADSCANPB_LOG);

    // First three channels are always the num frames, height, and then width
    setIntegerParam(ADScanPB_NumFrames, numFrames);
    setIntegerParam(ADMaxSizeX, xSize);
    setIntegerParam(ADSizeX, xSize);
    setIntegerParam(ADMaxSizeY, ySize);
    setIntegerParam(ADSizeY, ySize);

    size_t numElems = numFrames * ySize * xSize;

    if (bytesPerElem == 1) {
        setIntegerParam(NDDataType, NDUInt8);
    } else if (bytesPerElem == 2) {
        setIntegerParam(NDDataType, NDUInt16);
    } else {
        updateStatus("Couldn't read image dataset data type!", ADSCANPB_ERR);
        closeScan();
        return asynError;
    }

    callParamCallbacks();

    // allocate buffer for image data & read entire scan into it.
    this->scanImageDataBuffer = calloc(numElems, bytesPerElem);

    int framesLoaded = 0;

    int firstChunkListLen = chunks[0].size();
    int secondChunkListLen = chunks[1].size();
    size_t bufferWriteOffset = 0;
    for (int i = 0; i < firstChunkListLen; i++) {
        for (int j = 0; j < secondChunkListLen; j++) {
            int numAcquisitionsPerChunk = chunks[0][i].get<int>();
            int numFramesPerChunk = chunks[1][j].get<int>();

            cpr::Header dataHeader;
            if (this->tiledApiKey.empty()) {
                dataHeader = cpr::Header{{string("Accept"), string("application/octet-stream")}};
            } else {
                dataHeader = cpr::Header{{string("Authorization"), "Apikey " + this->tiledApiKey},
                                         {string("Accept"), string("application/octet-stream")}};
            }

            // TODO - Update to use block url read from metadata above

            char fullURLC[512];
            sprintf(fullURLC, "%s?block=%d,%d,0,0", dataURL.c_str(), i, j);
            string fullURL = string(fullURLC);

            cout << fullURL << endl;
            size_t numBytesToCopy =
                numAcquisitionsPerChunk * numFramesPerChunk * xSize * ySize * bytesPerElem;
            cout << numBytesToCopy << endl;

            char loadingMsg[256];
            sprintf(loadingMsg, "Loading chunk %d of %d...", (i * secondChunkListLen + j),
                    (firstChunkListLen * secondChunkListLen));
            updateStatus(loadingMsg, ADSCANPB_LOG);
            callParamCallbacks();

            cpr::Response data = cpr::Get(cpr::Url{fullURL}, cpr::ReserveSize{numBytesToCopy * 2},
                                          cpr::AcceptEncoding({{}}), dataHeader);

            if (data.status_code != 200) {
                updateStatus(data.text.c_str(), ADSCANPB_ERR);
                free(this->scanImageDataBuffer);
                return asynError;
            }

            /*cout << data.status_code << endl;
            cout << data.downloaded_bytes << endl;
            cout << data.raw_header << endl;*/

            memcpy((void *)((uint8_t *)this->scanImageDataBuffer + bufferWriteOffset),
                   (void *)data.text.c_str(), numBytesToCopy);
            framesLoaded = framesLoaded + (numAcquisitionsPerChunk * numFramesPerChunk);
            setIntegerParam(ADScanPB_NumFramesLoaded, framesLoaded);
            setDoubleParam(ADScanPB_LoadPercent, 100 * (framesLoaded / numFrames));
            callParamCallbacks();
            bufferWriteOffset += numBytesToCopy;
        }
    }

    updateStatus("Done", ADSCANPB_LOG);
    setIntegerParam(ADScanPB_ScanLoaded, 1);
    callParamCallbacks();
    return status;
}
#endif

asynStatus ADScanPB::openScanHDF5(const char *filePath) {
    const char *functionName = "openScanHDF5";
    asynStatus status = asynSuccess;

    hid_t fileId, imageDatasetId, tsDatasetId;

    LOG_ARGS("Attempting to open HDF5 file: %s", filePath);

    // Open H5 file and
    fileId = H5Fopen(filePath, H5F_ACC_RDONLY, H5P_DEFAULT);

    if (fileId < 0) {
        updateStatus("Failed to open HDF5 scan file!", ADSCANPB_ERR);
        return asynError;
    }

    char imageDataset[256];
    getStringParam(ADScanPB_ImageDataset, 256, (char *)imageDataset);

    imageDatasetId = H5Dopen(fileId, imageDataset, H5P_DEFAULT);

    if (imageDatasetId < 0) {
        updateStatus("Image dataset not found in file!", ADSCANPB_ERR);
        H5Fclose(fileId);
        return asynError;
    }

    char timestampDataset[256];
    getStringParam(ADScanPB_TSDataset, 256, (char *)timestampDataset);
    if (strlen(timestampDataset) > 0) {
        tsDatasetId = H5Dopen(fileId, timestampDataset, H5P_DEFAULT);

        if (tsDatasetId < 0) {
            WARN("Timestamp dataset could not be opened");
        } else {
            hid_t dspace = H5Dget_space(tsDatasetId);
            hsize_t dims[1];
            H5Sget_simple_extent_dims(dspace, dims, NULL);
            H5Dclose(dspace);

            scanTimestampDataBuffer = calloc(dims[0], sizeof(double));
            H5Dread(tsDatasetId, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT,
                    (double *)this->scanTimestampDataBuffer);
            H5Dclose(tsDatasetId);
        }
    }

    // Collect image dataset dimension information.
    hid_t dspace = H5Dget_space(imageDatasetId);
    const int ndims = H5Sget_simple_extent_ndims(dspace);
    hsize_t dims[ndims];
    H5Sget_simple_extent_dims(dspace, dims, NULL);
    H5Sclose(dspace);

    if (ndims == 3) {
        LOG_ARGS("Detected image dataset with %d dimensions: (%d, %d, %d)", ndims, (int)dims[0],
                 (int)dims[1], (int)dims[2]);
    } else if (ndims == 4) {
        LOG_ARGS("Detected image dataset with %d dimensions: (%d, %d, %d, %d)", ndims, (int)dims[0],
                 (int)dims[1], (int)dims[2], (int)dims[3]);
    } else {
        LOG_ARGS("Detected image dataset with %d dimensions.", ndims);
    }

    // Calculate total number of elements (pixels) in image dataset
    size_t num_elems = 1;
    for (int i = 0; i < ndims; i++) {
        num_elems = num_elems * dims[i];
    }

    // Number of frames in scan will always be the first dimension
    hsize_t numFrames = dims[0];

    updateStatus("Loading scan file...", ADSCANPB_LOG);

    // First three channels are always the num frames, height, and then width
    setIntegerParam(ADScanPB_NumFrames, numFrames);
    setIntegerParam(ADMaxSizeX, (int)dims[2]);
    setIntegerParam(ADSizeX, (int)dims[2]);
    setIntegerParam(ADMaxSizeY, (int)dims[1]);
    setIntegerParam(ADSizeY, (int)dims[1]);

    // determine whether or not the image data is in color or not.
    if (ndims == 4) {
        setIntegerParam(NDColorMode, NDColorModeRGB1);
    } else {
        setIntegerParam(NDColorMode, NDColorModeMono);
    }

    // Determine datatype of image data, and populate corresponding PVs
    hid_t h5_dtype = H5Dget_type(imageDatasetId);
    size_t dtype_size;
    if (H5Tequal(h5_dtype, H5T_NATIVE_UINT8) || H5Tequal(h5_dtype, H5T_NATIVE_UCHAR)) {
        setIntegerParam(NDDataType, NDUInt8);
        dtype_size = sizeof(uint8_t);
    } else if (H5Tequal(h5_dtype, H5T_NATIVE_UINT16)) {
        setIntegerParam(NDDataType, NDUInt16);
        dtype_size = sizeof(uint16_t);
    } else {
        updateStatus("Couldn't read image dataset data type!", ADSCANPB_ERR);
        H5Dclose(imageDatasetId);
        H5Dclose(tsDatasetId);
        H5Tclose(h5_dtype);
        H5Fclose(fileId);
        closeScan();
        return asynError;
    }

    callParamCallbacks();

    // allocate buffer for image data & read entire scan into it.
    this->scanImageDataBuffer = calloc(num_elems, dtype_size);
    H5Dread(imageDatasetId, h5_dtype, H5S_ALL, H5S_ALL, H5P_DEFAULT, this->scanImageDataBuffer);

    H5Tclose(h5_dtype);

    H5Dclose(imageDatasetId);
    H5Fclose(fileId);
    updateStatus("Done", ADSCANPB_LOG);
    // H5Dread does not offer any progress indicators.
    setIntegerParam(ADScanPB_NumFramesLoaded, numFrames);
    setDoubleParam(ADScanPB_LoadPercent, 100);
    setIntegerParam(ADScanPB_ScanLoaded, 1);
    callParamCallbacks();
    return status;
}


void ADScanPB::updateImageDatasetDesc(ADScanPBDataSource_t dataSource){
    const char* functionName = "updateImageDatasetDesc";
    char imgDatasetDesc[256];
    if (dataSource == ADSCANPB_DS_HDF5) sprintf(imgDatasetDesc, "Internal path to image dataset");
    else if (dataSource == ADSCANPB_DS_TIFF) sprintf(imgDatasetDesc, "Match pattern of tiff filenames");
    else if (dataSource == ADSCANPB_DS_JPEG) sprintf(imgDatasetDesc, "Match pattern of jpeg filenames");
    else if (dataSource == ADSCANPB_DS_MP4) sprintf(imgDatasetDesc, "N/A");
    else if (dataSource == ADSCANPB_DS_TILED) sprintf(imgDatasetDesc, "Tiled Metadata URL");
    else if (dataSource == ADSCANPB_DS_KAFKA) sprintf(imgDatasetDesc, "Kafka Topic");
    setStringParam(ADScanPB_ImageDatasetDesc, imgDatasetDesc);
}

asynStatus ADScanPB::writeOctet(asynUser *pasynUser, const char *value, size_t nChars,
                                 size_t *nActual) {
    int addr = 0;
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    const char *functionName = "writeOctet";

    status = getAddress(pasynUser, &addr);
    if (status != asynSuccess) return (status);
    // Set the parameter in the parameter library.
    status = (asynStatus)setStringParam(addr, function, (char *)value);
    if (status != asynSuccess) return (status);

    if (function == ADScanPB_ScanFilePath) {
        if ((nChars > 0) && (value[0] != 0)) {
            // If we have a scan loaded already, close it out first
            int scanLoaded;
            getIntegerParam(ADScanPB_ScanLoaded, &scanLoaded);
            if (scanLoaded == 1) closeScan();

            int dataSource;
            getIntegerParam(ADScanPB_DataSource, &dataSource);
            if (dataSource == ADSCANPB_DS_HDF5) status = this->openScanHDF5(value);
#ifdef ADSCANPB_WITH_TILED_SUPPORT
            else if (dataSource == 1)
                status = this->openScanTiled(value);
#endif
            else
                updateStatus("Selected data source not supported in current ADScanPB build!",
                             ADSCANPB_ERR);
        }
    }

    else if (function < ADSCANPB_FIRST_PARAM) {
        /* If this parameter belongs to a base class call its method */
        status = ADDriver::writeOctet(pasynUser, value, nChars, nActual);
    }
    // Do callbacks so higher layers see any changes
    callParamCallbacks(addr);

    *nActual = nChars;
    return status;
}

/*
 * Function used for reporting ADScanPB device and library information to a external
 * log file. The function first prints all libuvc specific information to the file,
 * then continues on to the base ADDriver 'report' function
 *
 * @params[in]: fp      -> pointer to log file
 * @params[in]: details -> number of details to write to the file
 * @return: void
 */
void ADScanPB::report(FILE *fp, int details) {
    const char *functionName = "report";
    int height;
    int width;
    LOG("Reporting to external log file");
    if (details > 0) {
        fprintf(fp, " Connected Device Information\n");
        getIntegerParam(ADSizeX, &width);
        getIntegerParam(ADSizeY, &height);
        fprintf(fp, " Image Width           ->      %d\n", width);
        fprintf(fp, " Image Height          ->      %d\n", height);
        fprintf(fp, " -------------------------------------------------------------------\n");
        fprintf(fp, "\n");

        ADDriver::report(fp, details);
    }
}

//----------------------------------------------------------------------------
// ADScanPB Constructor/Destructor
//----------------------------------------------------------------------------

ADScanPB::ADScanPB(const char *portName, int maxBuffers, size_t maxMemory, int priority,
                     int stackSize)
    : ADDriver(portName, 1, (int)NUM_SCANPB_PARAMS, maxBuffers, maxMemory, asynEnumMask,
               asynEnumMask, ASYN_CANBLOCK, 1, priority, stackSize) {
    static const char *functionName = "ADScanPB";

    LOG("Intializing scan playback tool...");

    /* Turn off HDF5 error handling */
    H5Eset_auto(H5E_DEFAULT, NULL, NULL);

    createParam(ADScanPB_PlaybackRateFPSString, asynParamFloat64, &ADScanPB_PlaybackRateFPS);
    createParam(ADScanPB_PlaybackRateSPFString, asynParamFloat64, &ADScanPB_PlaybackRateSPF);
    createParam(ADScanPB_ScanFilePathString, asynParamOctet, &ADScanPB_ScanFilePath);
#ifdef ADSCANPB_WITH_TILED_SUPPORT
    createParam(ADScanPB_TiledMetadataURLString, asynParamOctet, &ADScanPB_TiledMetadataURL);
#endif
    createParam(ADScanPB_DataSourceString, asynParamInt32, &ADScanPB_DataSource);
    createParam(ADScanPB_ImageDatasetString, asynParamOctet, &ADScanPB_ImageDataset);
    createParam(ADScanPB_ImageDatasetDescString, asynParamOctet, &ADScanPB_ImageDatasetDesc);
    createParam(ADScanPB_TSDatasetString, asynParamOctet, &ADScanPB_TSDataset);
    createParam(ADScanPB_AutoRepeatString, asynParamInt32, &ADScanPB_AutoRepeat);
    createParam(ADScanPB_ScanLoadedString, asynParamInt32, &ADScanPB_ScanLoaded);
    createParam(ADScanPB_PlaybackPosString, asynParamInt32, &ADScanPB_PlaybackPos);
    createParam(ADScanPB_ResetPlaybackPosString, asynParamInt32, &ADScanPB_ResetPlaybackPos);
    createParam(ADScanPB_NumFramesString, asynParamInt32, &ADScanPB_NumFrames);
    createParam(ADScanPB_SupportedSourcesString, asynParamInt32, &ADScanPB_SupportedSources);
    createParam(ADScanPB_NumFramesLoadedString, asynParamInt32, &ADScanPB_NumFramesLoaded);
    createParam(ADScanPB_LoadPercentString, asynParamFloat64, &ADScanPB_LoadPercent);
    createParam(ADScanPB_TriggerEdgeString, asynParamInt32, &ADScanPB_TriggerEdge);
    createParam(ADScanPB_IdleReadySignalString, asynParamInt32, &ADScanPB_IdleReadySignal);
    createParam(ADScanPB_ReadySignalString, asynParamInt32, &ADScanPB_ReadySignal);
    createParam(ADScanPB_TriggerSignalString, asynParamInt32, &ADScanPB_TriggerSignal);
    createParam(ADScanPB_NumTrigsRecdString, asynParamInt32, &ADScanPB_NumTrigsRecd);
    createParam(ADScanPB_NumTrigsDroppedString, asynParamInt32, &ADScanPB_NumTrigsDropped);

    int supportedDataSources = ADSCANPB_DS_HDF5; // Set Supported data sources to default builtins (HDF5)

    // Sets driver version PV (version numbers defined in header file)
    char versionString[25];
    epicsSnprintf(versionString, sizeof(versionString), "%d.%d.%d", ADSCANPB_VERSION,
                  ADSCANPB_REVISION, ADSCANPB_MODIFICATION);
    setStringParam(NDDriverVersion, versionString);

    char h5versionString[25];
    epicsSnprintf(h5versionString, sizeof(h5versionString), "%d.%d.%d", H5_VERS_MAJOR,
                  H5_VERS_MINOR, H5_VERS_RELEASE);
    setStringParam(ADSDKVersion, h5versionString);

    setStringParam(ADModel, "Scan Playback Tool");
    setStringParam(ADManufacturer, "NSLS2");
    setStringParam(ADFirmwareVersion, "N/A");
    setStringParam(ADSerialNumber, "N/A");

#ifdef ADSCANPB_WITH_TILED_SUPPORT

    supportedDataSources = supportedDataSources & ADSCANPB_DS_TILED;

    char metadataURL[256];

    // Load Tiled api key from env vars.
    if (getenv("TILED_API_KEY") != NULL) this->tiledApiKey = string(getenv("TILED_API_KEY"));

    if (getenv("TILED_METADATA_URL") != NULL)
        setStringParam(ADScanPB_ImageDataset, getenv("TILED_METADATA_URL"));

    getStringParam(ADScanPB_TiledMetadataURL, 256, metadataURL);

    // If all required tiled env vars are set, allow for opening data via tiled
    if (!this->tiledApiKey.empty() && strlen(metadataURL) != 0) this->tiledConfigured = true;
#endif

    setIntegerParam(ADScanPB_SupportedSources, supportedDataSources);

    int dataSource;
    getIntegerParam(ADScanPB_DataSource, &dataSource);

    updateImageDatasetDesc((ADScanPBDataSource_t) dataSource);

    // create events for rising/falling edge triggers
    this->risingEdgeEventId = epicsEventCreate(epicsEventEmpty);
    this->fallingEdgeEventId = epicsEventCreate(epicsEventEmpty);

    // when epics is exited, delete the instance of this class
    epicsAtExit(exitCallbackC, this);
}

ADScanPB::~ADScanPB() {
    const char *functionName = "~ADScanPB";
    LOG("Shutting down scan playback tool...");
    closeScan();
    LOG("Done.");
}

//-------------------------------------------------------------
// ADScanPB ioc shell registration
//-------------------------------------------------------------

/* ScanPBConfig -> These are the args passed to the constructor in the epics config function */
static const iocshArg ScanPBConfigArg0 = {"Port name", iocshArgString};
static const iocshArg ScanPBConfigArg1 = {"maxBuffers", iocshArgInt};
static const iocshArg ScanPBConfigArg2 = {"maxMemory", iocshArgInt};
static const iocshArg ScanPBConfigArg3 = {"priority", iocshArgInt};
static const iocshArg ScanPBConfigArg4 = {"stackSize", iocshArgInt};

/* Array of config args */
static const iocshArg *const ScanPBConfigArgs[] = {&ScanPBConfigArg0, &ScanPBConfigArg1,
                                                    &ScanPBConfigArg2, &ScanPBConfigArg3,
                                                    &ScanPBConfigArg4};

/* what function to call at config */
static void configScanPBCallFunc(const iocshArgBuf *args) {
    ADScanPBConfig(args[0].sval, args[1].ival, args[2].ival, args[3].ival, args[4].ival);
}

/* information about the configuration function */
static const iocshFuncDef configScanPB = {"ADScanPBConfig", 5, ScanPBConfigArgs};

/* IOC register function */
static void ScanPBRegister(void) { iocshRegister(&configScanPB, configScanPBCallFunc); }

/* external function for IOC register */
extern "C" {
epicsExportRegistrar(ScanPBRegister);
}
