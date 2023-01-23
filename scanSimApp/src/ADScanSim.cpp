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


// Area Detector include
#include "ADScanSim.h"


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
extern "C" int ADScanSimConfig(const char* portName, const char* connectionParam, int maxBuffers, size_t maxMemory, int priority, int stackSize){
    new ADScanSim(portName, connectionParam, maxBuffers, maxMemory, priority, stackSize);
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


/**
 * Simple function that prints all information about a connected camera
 * 
 * @return: void
 */
void ADScanSim::printConnectedDeviceInfo(){
    printf("--------------------------------------\n");
    printf("Connected to ScanSim device\n");
    printf("--------------------------------------\n");
    // Add any information you wish to print about the device here
    printf("--------------------------------------\n");
}


// -----------------------------------------------------------------------
// ADScanSim Connect/Disconnect Functions
// -----------------------------------------------------------------------


/**
 * Function that is used to initialize and connect to the device.
 * 
 * NOTE: Again, it is possible that for your camera, a different connection type is used (such as a product ID [int])
 * Make sure you use the same connection type as passed in the ADScanSimConfig function and in the constructor.
 * 
 * @params[in]: serialNumber    -> serial number of camera to connect to. Passed through IOC shell
 * @return:     status          -> success if connected, error if not connected
 */
asynStatus ADScanSim::connectToDeviceScanSim(const char* serialNumber){
    const char* functionName = "connectToDeviceScanSim";
    bool connected = false;


    // Implement connecting to the camera here
    // Usually the vendor provides examples of how to do this with the library/SDK


    if(connected) return asynSuccess;
    else{
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Error: failed to connect to device\n", driverName, functionName);
        return asynError;
    }
}


/**
 * Function that disconnects from any connected device
 * First checks if is connected, then if it is, it frees the memory
 * for the info and the camera
 * 
 * @return: status  -> success if freed, error if never connected
 */
asynStatus ADScanSim::disconnectFromDeviceScanSim(){
    const char* functionName = "disconnectFromDeviceScanSim";

    // Free up any data allocated by driver here, and call the vendor libary to disconnect

    return asynSuccess;
}


/**
 * Function that updates PV values with camera information
 * 
 * @return: status
 */
asynStatus ADScanSim::getDeviceInformation(){
    const char* functionName = "collectCameraInformation";
    asynStatus status = asynSuccess;
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s Collecting camera information\n", driverName, functionName);

    // Use the vendor library to collect information about the connected camera here, and set the appropriate PVs
    // Make sure you check if camera is connected before calling on it for information

    //setStringParam(ADManufacturer,        _____________);
    //setStringParam(ADSerialNumber,        _____________);
    //setStringParam(ADFirmwareVersion,     _____________);
    //setStringParam(ADModel,               _____________);

    return status;
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
    asynStatus status;
    if(this->pcamera == NULL){
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Error: No camera connected\n", driverName, functionName);
        status = asynError;
    }
    else{

        // Here, you need to start acquisition. Generally there are two setups for drivers. Either the vendor provides
        // a function that starts acquisition and takes a callback function to process frames on a different thread,
        // or you will need to create your own acquisition thread here. 

        if(status != asynSuccess){
            setIntegerParam(ADAcquire, 0);
            setIntegerParam(ADStatus, ADStatusIdle);
            callParamCallbacks();
            status = asynError;
        }
        else{
            setIntegerParam(ADStatus, ADStatusAcquire);
            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s Image acquistion start\n", driverName, functionName);
            callParamCallbacks();
        }

    }
    return status;
}




/**
 * Function responsible for stopping camera image acquisition. First check if the camera is connected.
 * If it is, execute the 'AcquireStop' command. Then set the appropriate PV values, and callParamCallbacks
 * 
 * @return: status  -> error if no camera or command fails to execute, success otherwise
 */ 
asynStatus ADScanSim::acquireStop(){
    const char* functionName = "acquireStop";
    asynStatus status;
    if(this->pcamera == NULL){
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Error: No camera connected\n", driverName, functionName);
        status = asynError;
    }
    else{
        // Here, you need to stop acquisition. If the vendor software spawned its own callback thread, you likely
        // just need to call some stop acquisition function. If you created your own callback thread, you should join
        // it here.
    }
    setIntegerParam(ADStatus, ADStatusIdle);
    setIntegerParam(ADAcquire, 0);
    callParamCallbacks();
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s Stopping Image Acquisition\n", driverName, functionName);
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
    int status = asynSuccess;
    static const char* functionName = "writeInt32";
    getIntegerParam(ADAcquire, &acquiring);

    status = setIntegerParam(function, value);
    // start/stop acquisition
    if(function == ADAcquire){
        if(value && !acquiring){
            deviceStatus = acquireStart();
            if(deviceStatus < 0){
                reportScanSimError(deviceStatus, functionName);
                return asynError;
            }
        }
        if(!value && acquiring){
            acquireStop();
        }
    }

    else if(function == ADImageMode)
        if(acquiring == 1) acquireStop();

    else{
        if (function < ADSCANSIM_FIRST_PARAM) {
            status = ADDriver::writeInt32(pasynUser, value);
        }
    }
    callParamCallbacks();

    if(status){
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s ERROR status=%d, function=%d, value=%d\n", driverName, functionName, status, function, value);
        return asynError;
    }
    else asynPrint(this->pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s::%s function=%d value=%d\n", driverName, functionName, function, value);
    return asynSuccess;
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
    int status = asynSuccess;
    static const char* functionName = "writeFloat64";
    getIntegerParam(ADAcquire, &acquiring);

    status = setDoubleParam(function, value);

    if(function == ADAcquireTime){
        if(acquiring) acquireStop();
    }
    else{
        if(function < ADScanSim_FIRST_PARAM){
            status = ADDriver::writeFloat64(pasynUser, value);
        }
    }
    callParamCallbacks();

    if(status){
        asynPrint(this-> pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s ERROR status = %d, function =%d, value = %f\n", driverName, functionName, status, function, value);
        return asynError;
    }
    else asynPrint(this->pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s::%s function=%d value=%f\n", driverName, functionName, function, value);
    return asynSuccess;
}



/*
 * Function used for reporting ADUVC device and library information to a external
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
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s reporting to external log file\n",driverName, functionName);
    if(details > 0){
        if(!connected){
            fprintf(fp, " No connected devices\n");
            ADDriver::report(fp, details);
            return;
        }
        fprintf(fp, " Connected Device Information\n");
        // GET CAMERA INFORMATION HERE AND PRINT IT TO fp
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


ADScanSim::ADScanSim(const char* portName, const char* connectionParam, int maxbuffers, size_t maxMemory, int priority, int stackSize )
    : ADDriver(portName, 1, (int)NUM_SCANSIM_PARAMS, maxBuffers, maxMemory, asynEnumMask, asynEnumMask, ASYN_CANBLOCK, 1, priority, stackSize){
    static const char* functionName = "ADScanSim";

    // Call createParam here for all of your 
    // ex. createParam(ADUVC_UVCComplianceLevelString, asynParamInt32, &ADUVC_UVCComplianceLevel);

    // Sets driver version PV (version numbers defined in header file) 
    char versionString[25];
    epicsSnprintf(versionString, sizeof(versionString), "%d.%d.%d", ADSCANSIM_VERSION, ADSCANSIM_REVISION, ADSCANSIM_MODIFICATION);
    setStringParam(NDDriverVersion, versionString);

    if(strlen(connectionParam) < 0){
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Connection failed, abort\n", driverName, functionName);
    }
    else{
        connected = connectToDeviceScanSim(serial);
        if(connected){
            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s Acquiring device information\n", driverName, functionName);
            getDeviceInformation();
        }
    }

     // when epics is exited, delete the instance of this class
    epicsAtExit(exitCallbackC, this);
}


ADScanSim::~ADScanSim(){
    const char* functionName = "~ADScanSim";
    disconnectFromDeviceScanSim();
    asynPrint(this->pasynUserSelf, ASYN_TRACEIO_DRIVER,"%s::%s ADScanSim\n", driverName, functionName);
    disconnect(this->pasynUserSelf);
}


//-------------------------------------------------------------
// ADScanSim ioc shell registration
//-------------------------------------------------------------

/* ScanSimConfig -> These are the args passed to the constructor in the epics config function */
static const iocshArg ScanSimConfigArg0 = { "Port name",        iocshArgString };

// This parameter must be customized by the driver author. Generally a URL, Serial Number, ID, IP are used to connect.
static const iocshArg ScanSimConfigArg1 = { "Connection Param", iocshArgString };

static const iocshArg ScanSimConfigArg2 = { "maxBuffers",       iocshArgInt };
static const iocshArg ScanSimConfigArg3 = { "maxMemory",        iocshArgInt };
static const iocshArg ScanSimConfigArg4 = { "priority",         iocshArgInt };
static const iocshArg ScanSimConfigArg5 = { "stackSize",        iocshArgInt };


/* Array of config args */
static const iocshArg * const ScanSimConfigArgs[] =
        { &ScanSimConfigArg0, &ScanSimConfigArg1, &ScanSimConfigArg2,
        &ScanSimConfigArg3, &ScanSimConfigArg4, &ScanSimConfigArg5 };


/* what function to call at config */
static void configScanSimCallFunc(const iocshArgBuf *args) {
    ADScanSimConfig(args[0].sval, args[1].sval, args[2].ival, args[3].ival, args[4].ival, args[5].ival);
}


/* information about the configuration function */
static const iocshFuncDef configUVC = { "ADScanSimConfig", 5, ScanSimConfigArgs };


/* IOC register function */
static void ScanSimRegister(void) {
    iocshRegister(&configScanSim, configScanSimCallFunc);
}


/* external function for IOC register */
extern "C" {
    epicsExportRegistrar(ScanSimRegister);
}
