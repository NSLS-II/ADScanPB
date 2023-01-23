/*
 * Header file for the ADScanSim EPICS driver
 *
 * This file contains the definitions of PV params, and the definition of the ADScanSim class and functions.
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
#ifndef ADSCANSIM_H
#define ADSCANSIM_H

// version numbers
#define ADScanSim_VERSION      0
#define ADScanSim_REVISION     0
#define ADScanSim_MODIFICATION 0


// Place PV string definitions here
// #define ADSCANSIMSHORT_PVNameString          "PV_NAME"            //asynInt32


// Place any required inclues here

#include "ADDriver.h"


// ----------------------------------------
// ScanSim Data Structures
//-----------------------------------------

// Place any in use Data structures here



/*
 * Class definition of the ADScanSim driver. It inherits from the base ADDriver class
 *
 * Includes constructor/destructor, PV params, function defs and variable defs
 *
 */
class ADScanSim : ADDriver{

    public:

        // Constructor - NOTE THERE IS A CHANCE THAT YOUR CAMERA DOESNT CONNECT WITH SERIAL # AND THIS MUST BE CHANGED
        ADScanSim(const char* portName, const char* connectionParam, int maxBuffers, size_t maxMemory, int priority, int stackSize);


        // ADDriver overrides
        virtual asynStatus writeInt32(asynUser* pasynUser, epicsInt32 value);
        virtual asynStatus writeFloat64(asynUser* pasynUser, epicsFloat64 value);


        // destructor. Disconnects from camera, deletes the object
        ~ADScanSim();

    protected:

        // Add PV indexes here. You must also define the first/last index as you add them.
        // Ex: int ADUVC_UVCCompliance;
        #define ADSCANSIM_FIRST_PARAM FIRST_PV_INDEX
        #define ADSCANSIM_LAST_PARAM LAST_PV_INDEX

    private:

        // Some data variables
        epicsEventId startEventId;
        epicsEventId endEventId;
        

        // ----------------------------------------
        // ScanSim Global Variables
        //-----------------------------------------


        // ----------------------------------------
        // ScanSim Functions - Logging/Reporting
        //-----------------------------------------

        //function used to report errors in scanSim operations
        // Note that vendor libraries usually have a status data structure, if not
        // it might be wise to make one
        void reportScanSimError(______ status, const char* functionName);

        // reports device and driver info into a log file
        void report(FILE* fp, int details);

        // writes to ADStatus PV
        void updateStatus(const char* status);

        // ----------------------------------------
        // UVC Functions - Connecting to camera
        //-----------------------------------------

        //function used for connecting to a ScanSim device
        // NOTE - THIS MAY ALSO NEED TO CHANGE IF SERIAL # NOT USED
        asynStatus connectToDeviceScanSim(const char* connectionParam);

        //function used to disconnect from ScanSim device
        asynStatus disconnectFromDeviceScanSim();

        // ----------------------------------------
        // ScanSim Functions - Camera functions
        //-----------------------------------------


        //function that begins image aquisition
        asynStatus acquireStart();

        //function that stops aquisition
        void acquireStop();

};

// Stores number of additional PV parameters are added by the driver
#define NUM_SCANSIM_PARAMS ((int)(&ADSCANSIM_LAST_PARAM - &ADSCANSIM_FIRST_PARAM + 1))

#endif
