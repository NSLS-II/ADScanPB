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
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "ERROR | %s::%s: %s\n", driverName, functionName, \
              msg)

#define ERR_ARGS(fmt, ...)                                                              \
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "ERROR | %s::%s: " fmt "\n", driverName, \
              functionName, __VA_ARGS__);

// Warning message formatters
#define WARN(msg) \
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "WARN | %s::%s: %s\n", driverName, functionName, msg)

#define WARN_ARGS(fmt, ...)                                                            \
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "WARN | %s::%s: " fmt "\n", driverName, \
              functionName, __VA_ARGS__);

// Log message formatters
#define LOG(msg) \
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s: %s\n", driverName, functionName, msg)

#define LOG_ARGS(fmt, ...)                                                                       \
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s: " fmt "\n", driverName, functionName, \
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


void ADScanSim::updateStatus(const char* msg, int errLevel){
    const char* functionName = "updateStatus";
    switch(errLevel){
        case ADSCANSIM_LOG:
            LOG(msg);
        case ADSCANSIM_WARN:
            WARN(msg);
        default:
            ERR(msg);
    }
    setStringParam(ADStatusMessage, msg);
}




// -----------------------------------------------------------------------
// ADScanSim Connect/Disconnect Functions
// -----------------------------------------------------------------------






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
    int dataType, imageMode, colorMode, ndims, autoRepeat, nframes;

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

        memcpy(pArray->pData, (uint16_t*) this->imageData + (width * height * (playbackPos)), width * height * 2);

        pArray->pAttributeList->add("ColorMode", "Color Mode", NDAttrInt32, &colorMode);

        getIntegerParam(NDArrayCounter, &imageCounter);
        imageCounter++;
        setIntegerParam(NDArrayCounter, imageCounter);


        getIntegerParam(ADNumImagesCounter, &totalImageCounter);
        totalImageCounter++;
        setIntegerParam(ADNumImagesCounter, totalImageCounter);
        pArray->uniqueId = totalImageCounter;


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

    // Here, you need to stop acquisition. If the vendor software spawned its own callback thread, you likely
    // just need to call some stop acquisition function. If you created your own callback thread, you should join
    // it here.
    if(this->playbackThreadId != NULL){
        this->playback = false;
        epicsThreadMustJoin(this->playbackThreadId);
    }

    setIntegerParam(ADStatus, ADStatusIdle);
    LOG("Stopping Image Acquisition");
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
    } else if(function == ADImageMode){
        if(acquiring == 1) acquireStop();
    } else if(function == NDDataType || function == NDColorMode){
        updateStatus("Color mode and data type are read from loaded scan", ADSCANSIM_ERR);
        status = asynError;
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


asynStatus ADScanSim::closeScan(){
    // If acquiring, stop acquiring first.
    int acquiring;
    getIntegerParam(ADAcquire, &acquiring);
    if(acquiring == 1)
        acquireStop();

    free(imageData);
    
    this->hstatus = H5Dclose(this->image_dset);
    //this->hstatus = H5Dclose(this->cm_dset);
    //this->hstatus = H5Dclose(this->ts_dset);
    //this->hstatus = H5Dclose(this->uid_dset);

    this->hstatus = H5Fclose(this->file);

    setIntegerParam(ADScanSim_ScanLoaded, 0);
}


asynStatus ADScanSim::openScan(const char* filePath){

    const char* functionName = "openScan";
    asynStatus status = asynSuccess;

    LOG_ARGS("Attempting to open file: %s", filePath);
    // If we have a scan loaded already, close it out first
    int scanLoaded;
    getIntegerParam(ADScanSim_ScanLoaded, &scanLoaded);
    if(scanLoaded == 1)
        closeScan();

    // Open H5 file and 
    this->file = H5Fopen(filePath, H5F_ACC_RDONLY, H5P_DEFAULT);

    if (this->file < 0) {
        updateStatus("Failed to open HDF5 scan file!", ADSCANSIM_ERR);
        return asynError;
    }

    this->image_dset = H5Dopen(file, "/img_tomo", H5P_DEFAULT);
    //this->ts_dset = H5Dopen(file, "/entry/instrument/NDAttributes/NDArrayTimeStamp", H5P_DEFAULT);
    //this->cm_dset = H5Dopen(file, "/entry/instrument/detector/NDAttributes/ColorMode", H5P_DEFAULT);
    //this->uid_dset = H5Dopen(file, "/entry/instrument/NDAttributes/NDArrayUniqueId", H5P_DEFAULT);

    hid_t dspace = H5Dget_space(image_dset);
    const int ndims = H5Sget_simple_extent_ndims(dspace);

    hsize_t dims[ndims];
    printf("NDIMS: %d\n", ndims);
    H5Sget_simple_extent_dims(dspace, dims, NULL);

    for(int i = 0; i< ndims; i++){
        printf("%d\n", dims[i]);
    }


    hsize_t numFrames = dims[0];

    setStringParam(ADStatusMessage, "Loading scan file...");
    setIntegerParam(ADScanSim_NumFrames, numFrames);
    printf("Here1\n");
    setIntegerParam(ADMaxSizeX, (int) dims[2]);
    setIntegerParam(ADSizeX, (int) dims[2]);
    printf("Here2\n");
    setIntegerParam(ADMaxSizeY, (int) dims[1]);
    setIntegerParam(ADSizeY, (int) dims[1]);
    printf("Here3\n");
    setIntegerParam(NDDataType, NDUInt16);
    printf("Here4\n");
    setIntegerParam(NDColorMode, NDColorModeMono);
    printf("Here5\n");
    callParamCallbacks();

    this->imageData = calloc(dims[0] * dims[1] * dims[2], sizeof(uint16_t));
    H5Dread(image_dset, H5T_NATIVE_UINT16, H5S_ALL, H5S_ALL, H5P_DEFAULT, this->imageData);
    for(int i = 0; i< 1000; i++){
        printf("Pixel #%d: %d\n", i, *((uint16_t*) imageData + i));
    }

    
    setStringParam(ADStatusMessage, "Done");
    setIntegerParam(ADScanSim_ScanLoaded, 1);
    printf("Here6\n");
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
            status = this->openScan(value);
        }
    }

    else if (function < ADSCANSIM_FIRST_PARAM) {
          /* If this parameter belongs to a base class call its method */
        status = ADDriver::writeOctet(pasynUser, value, nChars, nActual);
    }
    printf("Here7\n");
    // Do callbacks so higher layers see any changes
    callParamCallbacks(addr);
    printf("Here8\n");

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
    createParam(ADScanSim_AutoRepeatString, asynParamInt32, &ADScanSim_AutoRepeat);
    createParam(ADScanSim_ScanLoadedString, asynParamInt32, &ADScanSim_ScanLoaded);
    createParam(ADScanSim_PlaybackPosString, asynParamInt32, &ADScanSim_PlaybackPos);
    createParam(ADScanSim_NumFramesString, asynParamInt32, &ADScanSim_NumFrames);

    // Sets driver version PV (version numbers defined in header file) 
    char versionString[25];
    epicsSnprintf(versionString, sizeof(versionString), "%d.%d.%d", ADSCANSIM_VERSION, ADSCANSIM_REVISION, ADSCANSIM_MODIFICATION);
    setStringParam(NDDriverVersion, versionString);


     // when epics is exited, delete the instance of this class
    epicsAtExit(exitCallbackC, this);
}


ADScanSim::~ADScanSim(){
    const char* functionName = "~ADScanSim";

    closeScan();
    
    LOG("Shutting down Scan Simulator...");
    
    disconnect(this->pasynUserSelf);
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
