/**
 * Main source file for the ADScanSim EPICS driver 
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


// EPICS includes
#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsExit.h>
#include <epicsString.h>
#include <epicsStdio.h>
#include <iocsh.h>
#include <epicsExport.h>

#define H5Gcreate_vers 2
#define H5Dopen_vers 2

#include <stdlib.h>
#include <stdio.h>
#include <cmath>
#include <iostream>
#include <sstream>
#include <hdf5.h>
#include <sys/stat.h>


// Area Detector include
#include "ADScanSim.h"

// Error message formatters
#define ERR(msg)                                                                                 \
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "ERR  | %s::%s: %s\n", driverName, functionName, \
              msg)

#define ERR_ARGS(fmt, ...)                                                              \
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

#define LOG_ARGS(fmt, ...)                                                                       \
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "LOG  | %s::%s: " fmt "\n", driverName, functionName, \
              __VA_ARGS__);


using namespace std;

// Add any additional namespaces here


const char* driverName = "ADScanSim";

// Add any driver constants here


// -----------------------------------------------------------------------
// ADScanSim Utility Functions (Reporting/Logging/ExternalC)
// -----------------------------------------------------------------------


/*
 * External configuration function for ADScanSim.
 * Envokes the constructor to create a new ADScanSim object
 * This is the function that initializes the driver, and is called in the IOC startup script
 * 
 * NOTE: When implementing a new driver with ADDriverTemplate, your camera may use a different connection method than
 * a const char* connectionParam. Just edit the param to fit your device, and make sure to make the same edit to the constructor below
 *
 * @params[in]: all passed into constructor
 * @return:     status
 */
extern "C" int ADScanSimConfig(const char* portName, int maxBuffers, size_t maxMemory, int priority, int stackSize){
    new ADScanSim(portName, maxBuffers, maxMemory, priority, stackSize);
    return(asynSuccess);
}


/*
 * Callback function called when IOC is terminated.
 * Deletes created object
 *
 * @params[in]: pPvt -> pointer to the ADDRIVERNAMESTANDATD object created in ADScanSimConfig
 * @return:     void
 */
static void exitCallbackC(void* pPvt){
    ADScanSim* pScanSim = (ADScanSim*) pPvt;
    delete(pScanSim);
}

static void playbackThreadC(void* pPvt){
    ADScanSim* pScanSim = (ADScanSim*) pPvt;
    pScanSim->playbackThread();
}


void ADScanSim::updateStatus(const char* msg, ADScanSimErr_t errLevel){
    const char* functionName = "updateStatus";
    switch(errLevel){
        case ADSCANSIM_LOG:
            LOG(msg);
            break;
        case ADSCANSIM_WARN:
            WARN(msg);
            break;
        default:
            ERR(msg);
            break;
    }
    setStringParam(ADStatusMessage, msg);
}


// -----------------------------------------------------------------------
// ADScanSim Acquisition Functions
// -----------------------------------------------------------------------


/**
 * Function responsible for starting camera image acqusition. First, check if there is a
 * camera connected. Then, set camera values by reading from PVs. Then, we execute the 
 * Acquire Start command. if this command was successful, image acquisition started.
 * 
 * @return: status  -> error if no device, camera values not set, or execute command fails. Otherwise, success
 */
asynStatus ADScanSim::acquireStart(){
    const char* functionName = "acquireStart";
    asynStatus status = asynSuccess;

    int scanLoaded;
    getIntegerParam(ADScanSim_ScanLoaded, &scanLoaded);
    if(scanLoaded != 1){
        updateStatus("Scan has not been loaded for playback!", ADSCANSIM_ERR);
        status = asynError;
    }

    if(status != asynSuccess){
        setIntegerParam(ADAcquire, 0);
        setIntegerParam(ADStatus, ADStatusIdle);
    }
    else{
        setIntegerParam(ADStatus, ADStatusAcquire);
        LOG("Image acquistion start");

        epicsThreadOpts opts;
        opts.priority = epicsThreadPriorityMedium;
        opts.stackSize = epicsThreadGetStackSize(epicsThreadStackMedium);
        opts.joinable = 1;
        this->playback = true;

        this->playbackThreadId = epicsThreadCreateOpt("playbackThread", (EPICSTHREADFUNC) playbackThreadC, this, &opts);

    }

    return status;
}


void ADScanSim::playbackThread(){
    const char* functionName = "playbackThread";

    NDArray* pArray;
    NDArrayInfo arrayInfo;
    int dataType, imageMode, colorMode, ndims, autoRepeat, nframes, arrayCallbacks;

    // Three different frame counters.
    int playbackPos, imageCounter, totalImageCounter;


    getIntegerParam(NDColorMode, &colorMode);
    getIntegerParam(NDDataType, &dataType);
    getIntegerParam(ADScanSim_AutoRepeat, &autoRepeat);
    getIntegerParam(ADImageMode, &imageMode);
    getIntegerParam(ADScanSim_NumFrames, &nframes);


    int width, height;
    getIntegerParam(ADMaxSizeX, &width);
    getIntegerParam(ADMaxSizeY, &height);

    if((NDColorMode_t) colorMode == NDColorModeMono) ndims = 2;
    else ndims = 3;

    size_t dims[ndims];
    if(ndims == 2){
        dims[0] = width;
        dims[1] = height;
    }
    else{
        dims[0] = 3;
        dims[1] = width;
        dims[2] = height;
    }

    while(playback) {
        double spf;
        getDoubleParam(ADScanSim_PlaybackRateSPF, &spf);
        getIntegerParam(ADScanSim_PlaybackPos, &playbackPos);
        LOG_ARGS("Playing back frame %d from scan...", playbackPos);

        // allocate memory for a new NDArray, and set pArray to a pointer for this memory
        this->pArrays[0] = pNDArrayPool->alloc(ndims, dims, (NDDataType_t) dataType, 0, NULL);
    
        if(this->pArrays[0]!=NULL){ 
            pArray = this->pArrays[0];
        }
        else{
            this->pArrays[0]->release();
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
                    "%s::%s Unable to allocate array\n", driverName, functionName);
            return;
        }

        updateTimeStamp(&pArray->epicsTS);

        size_t num_elems = 1;
        for(int i = 0; i< ndims; i++){
            num_elems = num_elems * dims[i];
        }

        size_t totalBytes = num_elems;
        if(dataType == NDUInt8) {
            memcpy(pArray->pData, (uint8_t*) this->scanImageDataBuffer + (num_elems * (playbackPos)), num_elems);
        } else {
            totalBytes = totalBytes * 2;
            memcpy(pArray->pData, (uint16_t*) this->scanImageDataBuffer + (num_elems * (playbackPos)), totalBytes);
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
        if(this->scanTimestampDataBuffer == NULL) {
            pArray->timeStamp = (double)pArray->epicsTS.secPastEpoch 
                      + ((double)pArray->epicsTS.nsec * 1.0e-9);
        } else {
            pArray->timeStamp = *((double*) this->scanTimestampDataBuffer + playbackPos); 
        }


        getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
        if(arrayCallbacks)
            doCallbacksGenericPointer(pArray, NDArrayData, 0);

        pArray->release();

        epicsThreadSleep(spf);

        playbackPos++;

        if(imageMode == ADImageSingle){
            playback = false;
        } 

        else if(imageMode == ADImageMultiple) {
            int desiredImages;
            getIntegerParam(ADNumImages, &desiredImages);
            if(desiredImages <= imageCounter) playback = false;
        }

        else if(playbackPos == nframes){
            if(autoRepeat == 1)
                playbackPos = 0;
            else {
                playback = false;
            }
        }

        setIntegerParam(ADScanSim_PlaybackPos, playbackPos);

        if(!playback){
            setIntegerParam(ADAcquire, 0);
            setIntegerParam(ADStatus, ADStatusIdle);
        }
        callParamCallbacks();
    }
}



/**
 * Function responsible for stopping camera image acquisition. First check if the camera is connected.
 * If it is, execute the 'AcquireStop' command. Then set the appropriate PV values, and callParamCallbacks
 * 
 * @return: status  -> error if no camera or command fails to execute, success otherwise
 */ 
asynStatus ADScanSim::acquireStop(){
    const char* functionName = "acquireStop";
    asynStatus status = asynSuccess;

    // Stop acquistion and join back the playback thread
    if(this->playback){
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
// * A function that converts the data type and color mode of the vendor software supplied image structure into areaDetector NDDataType_t and NDColorMode_t
// * A function for that takes a pointer to a vendor software image data structure (your callback function - runs once per image)
// 
// If the vendor software is expecting a static function pointer as the callback parameter, you can create a static function with a void pointer as an argument
// cast that void pointer to type ADScanSim*, and call the callback function






//---------------------------------------------------------
// Base ScanSim Camera functionality
//---------------------------------------------------------


//  Add functions for getting/setting various camera settings (gain, exposure etc.) here


asynStatus ADScanSim::connect(asynUser* pasynUser){
    return asynSuccess;
}

asynStatus ADScanSim::disconnect(asynUser* pasynUser){
    const char* functionName = "disconnect";
    asynStatus status = pasynManager->exceptionDisconnect(this->pasynUserSelf);
    if (status) {
        ERR_ARGS("error calling pasynManager->exceptionDisconnect, error=%s", pasynUserSelf->errorMessage);
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
asynStatus ADScanSim::writeInt32(asynUser* pasynUser, epicsInt32 value){
    int function = pasynUser->reason;
    int acquiring;
    asynStatus status = asynSuccess;
    static const char* functionName = "writeInt32";
    getIntegerParam(ADAcquire, &acquiring);

    status = setIntegerParam(function, value);
    // start/stop acquisition
    if(function == ADAcquire){
        if(value && !acquiring){
            status = acquireStart();
            if(status == asynError){
                updateStatus("Failed to start acquisition", ADSCANSIM_ERR);
                status = asynError;
            }
        }
        if(!value && acquiring){
            status = acquireStop();
        }
    } else if(function == ADScanSim_ResetPlaybackPos) {
        setIntegerParam(ADScanSim_PlaybackPos, 0);
    } else if(function == ADImageMode){
        if(acquiring == 1) acquireStop();
    } else if(function == NDDataType || function == NDColorMode){
        updateStatus("Color mode and data type are read from loaded scan", ADSCANSIM_ERR);
        status = asynError;
    } else if(function == ADStatus) {
        if(value == ADStatusIdle) printf("SAW STAT TO IDLE");
    } else{
        if (function < ADSCANSIM_FIRST_PARAM) {
            status = ADDriver::writeInt32(pasynUser, value);
        }
    }
    callParamCallbacks();

    if(status){
        ERR_ARGS("status=%d, function=%d, value=%d", status, function, value);
        return asynError;
    }
    else LOG_ARGS("function=%d value=%d", function, value);
    return status;
}


void ADScanSim::setPlaybackRate(int rateFormat){
    const char* functionName = "setPlaybackRate";
    double fps, spf;


    if(rateFormat == ADScanSim_PlaybackRateFPS){
        getDoubleParam(rateFormat, &fps);
        spf = 1 / fps;
        setDoubleParam(ADScanSim_PlaybackRateSPF, spf);
    } else{
        getDoubleParam(rateFormat, &spf);
        fps = 1 / spf;
        setDoubleParam(ADScanSim_PlaybackRateFPS, fps);
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
asynStatus ADScanSim::writeFloat64(asynUser* pasynUser, epicsFloat64 value){
    int function = pasynUser->reason;
    int acquiring;
    asynStatus status = asynSuccess;
    static const char* functionName = "writeFloat64";
    getIntegerParam(ADAcquire, &acquiring);

    status = setDoubleParam(function, value);

    if(function == ADScanSim_PlaybackRateFPS || function == ADScanSim_PlaybackRateSPF){
        setPlaybackRate(function);
    }
    else{
        if(function < ADSCANSIM_FIRST_PARAM){
            status = ADDriver::writeFloat64(pasynUser, value);
        }
    }
    callParamCallbacks();

    if(status){
        ERR_ARGS("status = %d, function =%d, value = %f", status, function, value);
        return asynError;
    }
    else LOG_ARGS("function=%d value=%f", function, value);
    return status;
}


void ADScanSim::closeScan(){
    // If acquiring, stop acquiring first.
    if(this->playback)
        acquireStop();

    // clear out buffers if they have been allocated
    if(this->scanImageDataBuffer != NULL)
        free(this->scanImageDataBuffer);

    if(this->scanTimestampDataBuffer != NULL)
        free(this->scanTimestampDataBuffer);

    setIntegerParam(ADScanSim_ScanLoaded, 0);
    callParamCallbacks();
}

#ifdef ADSCANSIM_WITH_TILED_SUPPORT
asynStatus ADScanSim::openScanTiled(const char* nodePath) {
    const char* functionName = "openScanTiled";
    asynStatus status = asynSuccess;

    char metadataURL[256];
    char arrayURL[256];
    getStringParam(ADScanSim_TiledMetadataURL, 256, metadataURL);
    getStringParam(ADScanSim_TiledArrayURL, 256, arrayURL);
    printf("%s\n", metadataURL);

    if(!this->tiledConfigured){
        updateStatus("Tiled configuration incomplete!", ADSCANSIM_ERR);
        return asynError;
    }

    LOG_ARGS("Attempting to load scan from Tiled node: %s", nodePath);

    cpr::Header auth = cpr::Header{{string("Authorization"), "Apikey " +  this->tiledApiKey}};
    cpr::Response r = cpr::Get(cpr::Url{string(metadataURL) + string(nodePath)}, auth);

    cout << r.text << endl;

    json metadata_j = json::parse(r.text.c_str());
    json scanShape = metadata_j["data"]["attributes"]["structure"]["macro"]["shape"];
    int numAcquistions = scanShape[0].get<int>();
    int numFrames = scanShape[1].get<int>() * numAcquistions;
    int ySize = scanShape[2].get<int>();
    int xSize = scanShape[3].get<int>();
    int bytesPerElem = metadata_j["data"]["attributes"]["structure"]["micro"]["itemsize"].get<int>();
    json chunks = metadata_j["data"]["attributes"]["structure"]["macro"]["chunks"];

    string dataURL = metadata_j["data"]["links"]["block"];
    char* dataURLToken = strtok((char*) dataURL.c_str(), "?");
    dataURL = string(dataURLToken);
    cout << dataURL << endl;

    // determine whether or not the image data is in color or not.
    //if(colorChannels == 3){
    //    setIntegerParam(NDColorMode, NDColorModeRGB1);
    //} else {
        setIntegerParam(NDColorMode, NDColorModeMono);
    //}

    updateStatus("Loading scan from URL...", ADSCANSIM_LOG);

    // First three channels are always the num frames, height, and then width
    setIntegerParam(ADScanSim_NumFrames, numFrames);
    setIntegerParam(ADMaxSizeX, xSize);
    setIntegerParam(ADSizeX, xSize);
    setIntegerParam(ADMaxSizeY, ySize);
    setIntegerParam(ADSizeY, ySize);

    size_t numElems = numFrames * ySize * xSize;

    if(bytesPerElem == 1) {
        setIntegerParam(NDDataType, NDUInt8);
    } else if(bytesPerElem == 2) {
        setIntegerParam(NDDataType, NDUInt16);
    } else {
        updateStatus("Couldn't read image dataset data type!", ADSCANSIM_ERR);
        closeScan();
        return asynError;
    }

    callParamCallbacks();

    // allocate buffer for image data & read entire scan into it.
    this->scanImageDataBuffer = calloc(numElems, bytesPerElem);

    int firstChunkListLen = chunks[0].size();
    int secondChunkListLen = chunks[1].size();
    size_t bufferWriteOffset = 0;
    for(int i = 0; i< firstChunkListLen; i++) {
        for(int j = 0; j< secondChunkListLen; j++) {

            int numAcquisitionsPerChunk = chunks[0][i].get<int>();
            int numFramesPerChunk = chunks[1][j].get<int>();

            cpr::Header dataHeader = cpr::Header{{string("Authorization"), "Apikey " +  this->tiledApiKey}, 
                                             {string("Accept"), string("application/octet-stream")}};

            // TODO - Update to use block url read from metadata above
            cpr::Response data = cpr::Get(cpr::Url{string(arrayURL) + string(nodePath)}, 
                               cpr::ReserveSize{numElems * bytesPerElem},
                               //cpr::AcceptEncoding({{"deflate", "gzip", "zlib"}}),
                               dataHeader);
            size_t numBytesToCopy = numAcquisitionsPerChunk * numFramesPerChunk * xSize * ySize * bytesPerElem;
            bufferWriteOffset += numBytesToCopy;
            memcpy((uint8_t*) this->scanImageDataBuffer + bufferWriteOffset, (void*) data.text.c_str(), numBytesToCopy);
        }
    }



    updateStatus("Done", ADSCANSIM_LOG);
    setIntegerParam(ADScanSim_ScanLoaded, 1);
    callParamCallbacks();
    return status;
}
#endif

asynStatus ADScanSim::openScanHDF5(const char* filePath){

    const char* functionName = "openScanHDF5";
    asynStatus status = asynSuccess;

    hid_t fileId, imageDatasetId, tsDatasetId;


    LOG_ARGS("Attempting to open HDF5 file: %s", filePath);

    // Open H5 file and 
    fileId = H5Fopen(filePath, H5F_ACC_RDONLY, H5P_DEFAULT);

    if (fileId < 0) {
        updateStatus("Failed to open HDF5 scan file!", ADSCANSIM_ERR);
        return asynError;
    }

    char imageDataset[256];
    getStringParam(ADScanSim_ImageDataset, 256, (char*) imageDataset);

    imageDatasetId = H5Dopen(fileId, imageDataset, H5P_DEFAULT);

    if(imageDatasetId < 0){
        updateStatus("Image dataset not found in file!", ADSCANSIM_ERR);
        H5Fclose(fileId);
        return asynError;
    }

    char timestampDataset[256];
    getStringParam(ADScanSim_TSDataset, 256, (char*) timestampDataset);
    if(strlen(timestampDataset) > 0){
        tsDatasetId = H5Dopen(fileId, timestampDataset, H5P_DEFAULT);

        if(tsDatasetId < 0){
            WARN("Timestamp dataset could not be opened");
        } else {
            hid_t dspace = H5Dget_space(tsDatasetId);
            hsize_t dims[1];
            H5Sget_simple_extent_dims(dspace, dims, NULL);
            H5Dclose(dspace);

            scanTimestampDataBuffer = calloc(dims[0], sizeof(double));
            H5Dread(tsDatasetId, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, (double*) this->scanTimestampDataBuffer);
            H5Dclose(tsDatasetId);
        }

    }

    hid_t dspace = H5Dget_space(imageDatasetId);
    const int ndims = H5Sget_simple_extent_ndims(dspace);

    hsize_t dims[ndims];
    printf("Detected image dataset with %d dimensions:\n", ndims);
    H5Sget_simple_extent_dims(dspace, dims, NULL);
    H5Dclose(dspace);

    size_t num_elems = 1;
    for(int i = 0; i< ndims; i++){
        printf("%d\n", dims[i]);
        num_elems = num_elems * dims[i];
    }

    // Number of frames in scan will always be the first dimension
    hsize_t numFrames = dims[0];

    updateStatus("Loading scan file...", ADSCANSIM_LOG);

    // First three channels are always the num frames, height, and then width
    setIntegerParam(ADScanSim_NumFrames, numFrames);
    setIntegerParam(ADMaxSizeX, (int) dims[2]);
    setIntegerParam(ADSizeX, (int) dims[2]);
    setIntegerParam(ADMaxSizeY, (int) dims[1]);
    setIntegerParam(ADSizeY, (int) dims[1]);

    // determine whether or not the image data is in color or not.
    if(ndims == 4){
        setIntegerParam(NDColorMode, NDColorModeRGB1);
    } else {
        setIntegerParam(NDColorMode, NDColorModeMono);
    }

    // Determine datatype of image data, and populate corresponding PVs
    hid_t h5_dtype = H5Dget_type(imageDatasetId);
    size_t dtype_size;
    if(H5Tequal(h5_dtype, H5T_NATIVE_UINT8) || H5Tequal(h5_dtype, H5T_NATIVE_UCHAR)) {
        setIntegerParam(NDDataType, NDUInt8);
        dtype_size = sizeof(uint8_t);
    } else if(H5Tequal(h5_dtype, H5T_NATIVE_UINT16)) {
        setIntegerParam(NDDataType, NDUInt16);
        dtype_size = sizeof(uint16_t);
    } else {
        updateStatus("Couldn't read image dataset data type!", ADSCANSIM_ERR);
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
    updateStatus("Done", ADSCANSIM_LOG);
    setIntegerParam(ADScanSim_ScanLoaded, 1);
    callParamCallbacks();
    return status;

}


asynStatus ADScanSim::writeOctet(asynUser* pasynUser, const char* value, size_t nChars, size_t* nActual){
    int addr=0;
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    const char *functionName = "writeOctet";

    status = getAddress(pasynUser, &addr); if (status != asynSuccess) return(status);
    // Set the parameter in the parameter library.
    status = (asynStatus)setStringParam(addr, function, (char *)value);
    if (status != asynSuccess) return(status);

    if (function == ADScanSim_ScanFilePath) {
        if ((nChars > 0) && (value[0] != 0)) {
            
            // If we have a scan loaded already, close it out first
            int scanLoaded;
            getIntegerParam(ADScanSim_ScanLoaded, &scanLoaded);
            if(scanLoaded == 1)
                closeScan();

            int dataSource;
            getIntegerParam(ADScanSim_DataSource, &dataSource);
            if(dataSource == 0)
                status = this->openScanHDF5(value);
#ifdef ADSCANSIM_WITH_TILED_SUPPORT
            else if(dataSource == 1)
                status = this->openScanTiled(value);
#endif
            else
                updateStatus("Selected data source not supported in current ADScanSim build!", ADSCANSIM_ERR);
        }
    }

    else if (function < ADSCANSIM_FIRST_PARAM) {
          /* If this parameter belongs to a base class call its method */
        status = ADDriver::writeOctet(pasynUser, value, nChars, nActual);
    }
    // Do callbacks so higher layers see any changes
    callParamCallbacks(addr);

    *nActual = nChars;
    return status;
}



/*
 * Function used for reporting ADScanSim device and library information to a external
 * log file. The function first prints all libuvc specific information to the file,
 * then continues on to the base ADDriver 'report' function
 * 
 * @params[in]: fp      -> pointer to log file
 * @params[in]: details -> number of details to write to the file
 * @return: void
 */
void ADScanSim::report(FILE* fp, int details){
    const char* functionName = "report";
    int height;
    int width;
    LOG("Reporting to external log file");
    if(details > 0){
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
// ADScanSim Constructor/Destructor
//----------------------------------------------------------------------------


ADScanSim::ADScanSim(const char* portName, int maxBuffers, size_t maxMemory, int priority, int stackSize )
    : ADDriver(portName, 1, (int)NUM_SCANSIM_PARAMS, maxBuffers, maxMemory, asynEnumMask, asynEnumMask, ASYN_CANBLOCK, 1, priority, stackSize){
    static const char* functionName = "ADScanSim";

    // Call createParam here for all of your 
    // ex. createParam(ADUVC_UVCComplianceLevelString, asynParamInt32, &ADUVC_UVCComplianceLevel);

    LOG("Intializing Scan Simulator...");

    /* Turn off HDF5 error handling */
    H5Eset_auto(H5E_DEFAULT, NULL, NULL);

    createParam(ADScanSim_PlaybackRateFPSString, asynParamFloat64, &ADScanSim_PlaybackRateFPS);
    createParam(ADScanSim_PlaybackRateSPFString, asynParamFloat64, &ADScanSim_PlaybackRateSPF);
    createParam(ADScanSim_ScanFilePathString, asynParamOctet, &ADScanSim_ScanFilePath);
#ifdef ADSCANSIM_WITH_TILED_SUPPORT
    createParam(ADScanSim_TiledMetadataURLString, asynParamOctet, &ADScanSim_TiledMetadataURL);
    createParam(ADScanSim_TiledArrayURLString, asynParamOctet, &ADScanSim_TiledArrayURL);
#endif
    createParam(ADScanSim_DataSourceString, asynParamInt32, &ADScanSim_DataSource);
    createParam(ADScanSim_ImageDatasetString, asynParamOctet, &ADScanSim_ImageDataset);
    createParam(ADScanSim_TSDatasetString, asynParamOctet, &ADScanSim_TSDataset);
    createParam(ADScanSim_AutoRepeatString, asynParamInt32, &ADScanSim_AutoRepeat);
    createParam(ADScanSim_ScanLoadedString, asynParamInt32, &ADScanSim_ScanLoaded);
    createParam(ADScanSim_PlaybackPosString, asynParamInt32, &ADScanSim_PlaybackPos);
    createParam(ADScanSim_ResetPlaybackPosString, asynParamInt32, &ADScanSim_ResetPlaybackPos);
    createParam(ADScanSim_NumFramesString, asynParamInt32, &ADScanSim_NumFrames);

    // Sets driver version PV (version numbers defined in header file) 
    char versionString[25];
    epicsSnprintf(versionString, sizeof(versionString), "%d.%d.%d", ADSCANSIM_VERSION, ADSCANSIM_REVISION, ADSCANSIM_MODIFICATION);
    setStringParam(NDDriverVersion, versionString);

    char h5versionString[25];
    epicsSnprintf(h5versionString, sizeof(h5versionString), "%d.%d.%d", H5_VERS_MAJOR, H5_VERS_MINOR, H5_VERS_RELEASE);
    setStringParam(ADSDKVersion, h5versionString);

    setStringParam(ADModel, "Scan Playback Tool");
    setStringParam(ADManufacturer, "BNL - NSLS2");
    setStringParam(ADFirmwareVersion, "N/A");
    setStringParam(ADSerialNumber, "N/A");

#ifdef ADSCANSIM_WITH_TILED_SUPPORT
    char metadataURL[256];

    // Load Tiled api key from env vars.
    if(getenv("TILED_API_KEY") != NULL)
        this->tiledApiKey = string(getenv("TILED_API_KEY"));

    if(getenv("TILED_METADATA_URL") != NULL)
        setStringParam(ADScanSim_TiledMetadataURL, getenv("TILED_METADATA_URL"));

    getStringParam(ADScanSim_TiledMetadataURL, 256, metadataURL);

    // If all required tiled env vars are set, allow for opening data via tiled
    if(!this->tiledApiKey.empty() && strlen(metadataURL) != 0)
        this->tiledConfigured = true;
#endif

     // when epics is exited, delete the instance of this class
    epicsAtExit(exitCallbackC, this);
}


ADScanSim::~ADScanSim(){
    const char* functionName = "~ADScanSim";
    LOG("Shutting down Scan Simulator...");
    closeScan();
    LOG("Done.");
}


//-------------------------------------------------------------
// ADScanSim ioc shell registration
//-------------------------------------------------------------


/* ScanSimConfig -> These are the args passed to the constructor in the epics config function */
static const iocshArg ScanSimConfigArg0 = { "Port name",        iocshArgString };
static const iocshArg ScanSimConfigArg1 = { "maxBuffers",       iocshArgInt };
static const iocshArg ScanSimConfigArg2 = { "maxMemory",        iocshArgInt };
static const iocshArg ScanSimConfigArg3 = { "priority",         iocshArgInt };
static const iocshArg ScanSimConfigArg4 = { "stackSize",        iocshArgInt };


/* Array of config args */
static const iocshArg * const ScanSimConfigArgs[] =
        { &ScanSimConfigArg0, &ScanSimConfigArg1, &ScanSimConfigArg2,
        &ScanSimConfigArg3, &ScanSimConfigArg4 };


/* what function to call at config */
static void configScanSimCallFunc(const iocshArgBuf *args) {
    ADScanSimConfig(args[0].sval, args[1].ival, args[2].ival, args[3].ival, args[4].ival);
}


/* information about the configuration function */
static const iocshFuncDef configScanSim = { "ADScanSimConfig", 5, ScanSimConfigArgs };


/* IOC register function */
static void ScanSimRegister(void) {
    iocshRegister(&configScanSim, configScanSimCallFunc);
}


/* external function for IOC register */
extern "C" {
    epicsExportRegistrar(ScanSimRegister);
}
