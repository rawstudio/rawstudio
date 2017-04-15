/***************************************************
 nikon_curve.h - read Nikon NTC/NCV files

 Copyright 2004-2015 by Shawn Freeman, Udi Fuchs

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

****************************************************/

/***************************************************

  This program reads in a Nikon NTC/NCV file,
  interperates it's tone curve, and writes out a
  simple ascii file containing a table of interpolation
  values.

  You'll note that this has been written in way that can be used in a
  standalone program or incorporated into another program. You can use the
  functions seperately or you can just call ConvertNikonCurveData with
  an input and output file.

  I've tried to document the code as clearly as possible. Let me know if you
  have any problems!

  Thanks goes out to Udi Fuchs for wanting to incorporate nikon curve loading
  into his program. This will make GIMP just that much better. :)

  @author: Shawn Freeman 1/06/2005
  @liscense: GNU GPL
****************************************************/
#ifndef _NIKON_CURVE_H
#define _NIKON_CURVE_H

#define NC_VERSION "1.2"
#define NC_DATE "2005-08-06"

#define NIKON_MAX_ANCHORS   20

//file types
#define NTC_FILE        0
#define NCV_FILE        1
#define NUM_FILE_TYPES  2

//Curve Types
#define TONE_CURVE      0
#define RED_CURVE       1
#define GREEN_CURVE     2
#define BLUE_CURVE      3
#define NUM_CURVE_TYPES 4

////////////////////////
//ERROR HANDLING
////////////////////////
#define NC_SUCCESS	0
#define NC_ERROR	100
#define NC_WARNING	104
#define NC_SET_ERROR	200


//////////////////////////////////////////////////////////////////////////////
//DATA STRUCTURES
//////////////////////////////////////////////////////////////////////////////

/**********************************************************
CurveData:
    Structure for the curve data inside a NTC/NCV file.
***********************************************************/
typedef struct {
    double x;
    double y;
} CurveAnchorPoint;

typedef struct {
    char name[80];

    //Type for this curve
    unsigned int m_curveType;

    //Box data
    double m_min_x;
    double m_max_x;
    double m_min_y;
    double m_max_y;
    double m_gamma;

    //Number of anchor points
    unsigned char m_numAnchors;

    //contains a list of anchors, 2 doubles per each point, x-y format
    //max is 20 points
    CurveAnchorPoint m_anchors[NIKON_MAX_ANCHORS];

} CurveData;

typedef struct {
    //Number of samples to use for the curve.
    unsigned int m_samplingRes;
    unsigned int m_outputRes;

    //Sampling array
    unsigned int *m_Samples;

} CurveSample;

/*********************************************
NikonData:
    Overall data structure for Nikon file data
**********************************************/
typedef struct {
    //Number of output points
    int m_fileType;
    unsigned short m_patch_version;
    CurveData curves[4];
} NikonData;

//////////////////////////////////////////////////////////////////////////////
//FUNCTIONS
//////////////////////////////////////////////////////////////////////////////

/*********************************************
CurveDataSample:
    Samples from a spline curve constructed from
    the curve data.

    curve   - Pointer to curve struct to hold the data.
    sample  - Pointer to sample struct to hold the data.
**********************************************/
int CurveDataSample(CurveData *curve, CurveSample *sample);

/*********************************************
 * CurveDataReset:
 *     Reset curve to straight line but don't touch the curve name.
 **********************************************/
void CurveDataReset(CurveData *curve);

/*********************************************
 * CurveDataIsTrivial:
 *     Check if the curve is a trivial linear curve.
 ***********************************************/
int CurveDataIsTrivial(CurveData *curve);

/*********************************************
 CurveDataSetPoint:
    Change the position of point to the new (x,y) coordinate.
    The end-points get a special treatment. When these are moved all the
    other points are moved together, keeping their relative position constant.
**********************************************/
void CurveDataSetPoint(CurveData *curve, int point, double x, double y);

/*******************************************************
CurveSampleInit:
    Init and allocate curve sample.
********************************************************/
CurveSample *CurveSampleInit(unsigned int samplingRes, unsigned int outputRes);

/*******************************************************
CurveSampleFree:
    Frees memory allocated for this curve sample.
********************************************************/
int CurveSampleFree(CurveSample *sample);

/*********************************************
LoadNikonData:
    Loads a curve from a Nikon ntc or ncv file.

    fileName    - The filename.
    curve        - Pointer to curve struct to hold the data.
    resolution    - How many data points to sample from the curve
**********************************************/
int LoadNikonData(char *fileName, NikonData *data);

/************************************************************
SaveNikonDataFile:
    Savess a curve to a Nikon ntc or ncv file.

    data        - A NikonData structure containing info of all the curves.
    fileName    - The filename.
    filetype    - Indicator for an NCV or NTC file.
**************************************************************/
int SaveNikonDataFile(NikonData *data, char *outfile, int filetype);

/*******************************************************
RipNikonNEFCurve:
    The actual retriever for the curve data from the NEF
    file.

    file   -	The input file.
    infile -	Offset to retrieve the data
    curve  -	data structure to hold curve in.
    sample_p -  pointer to the curve sample reference.
		can be NULL if curve sample is not needed.
********************************************************/
int RipNikonNEFCurve(void *file, int offset, CurveData *data,
                     CurveSample **sample_p);

#endif
