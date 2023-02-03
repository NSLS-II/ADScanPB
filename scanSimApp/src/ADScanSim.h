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
#define ADSCANSIM_VERSION      0
#define ADSCANSIM_REVISION     0
#define ADSCANSIM_MODIFICATION 0

#define ADSCANSIM_LOG 0
#define ADSCANSIM_WARN 1
#define ADSCANSIM_ERR 2


// Place PV string definitions here
#define ADScanSim_PlaybackRateFPSString          "PLAYBACK_RATE_FPS"            //
#define ADScanSim_PlaybackRateSPFString          "PLAYBACK_RATE_SPF"            //
#define ADScanSim_ScanFilePathString          "SCAN_FILE_PATH"            //
#define ADScanSim_AutoRepeatString            "AUTO_REPEAT"            //
#define ADScanSim_ScanLoadedString            "SCAN_LOADED"            //
#define ADScanSim_PlaybackPosString    "PLAYBACK_POS"
#define ADScanSim_NumFramesString             "NUM_FRAMES"            //


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
        ADScanSim(const char* portName, int maxBuffers, size_t maxMemory, int priority, int stackSize);


        // ADDriver overrides
        virtual asynStatus writeInt32(asynUser* pasynUser, epicsInt32 value);
        virtual asynStatus writeFloat64(asynUser* pasynUser, epicsFloat64 value);
        virtual asynStatus writeOctet(asynUser* pasynUser, const char* value, size_t nChars, size_t* nActual);


        // destructor. Disconnects from camera, deletes the object
        ~ADScanSim();

        void playbackThread();

    protected:

        // Add PV indexes here. You must also define the first/last index as you add them.
        // Ex: int ADUVC_UVCCompliance;
        int ADScanSim_PlaybackRateFPS;
        #define ADSCANSIM_FIRST_PARAM ADScanSim_PlaybackRateFPS
        int ADScanSim_PlaybackRateSPF;
        int ADScanSim_ScanFilePath;
        int ADScanSim_AutoRepeat;
        int ADScanSim_ScanLoaded;
        int ADScanSim_PlaybackPos;
        int ADScanSim_NumFrames;
        #define ADSCANSIM_LAST_PARAM ADScanSim_NumFrames

    private:

        // Some data variables
        epicsEventId startEventId;
        epicsEventId endEventId;

        herr_t hstatus;
        hid_t file, image_dset, ts_dset, cm_dset, uid_dset;


        void* imageData;

        bool playback = false;

        epicsThreadId playbackThreadId;

        // ----------------------------------------
        // ScanSim Functions - Logging/Reporting
        //-----------------------------------------


        // reports device and driver info into a log file
        void report(FILE* fp, int details);

        // writes to ADStatus PV
        void updateStatus(const char* status, int errLevel);

        // ----------------------------------------
        // ScanSim Functions - Simulator functions
        //-----------------------------------------

        asynStatus openScan(const char* filePath);
        asynStatus closeScan();



        void setPlaybackRate(int rateFormat);

        //function that begins image aquisition
        asynStatus acquireStart();

        //function that stops aquisition
        asynStatus acquireStop();

};

// Stores number of additional PV parameters are added by the driver
#define NUM_SCANSIM_PARAMS ((int)(&ADSCANSIM_LAST_PARAM - &ADSCANSIM_FIRST_PARAM + 1))

#endif
