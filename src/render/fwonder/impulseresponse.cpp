/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
 *                                                                                   *
 *  WONDER - Wave field synthesis Of New Dimensions of Electronic music in Realtime  *
 *  http://swonder.sourceforge.net                                                   *
 *                                                                                   *
 *                                                                                   *
 *  Technische Universität Berlin, Germany                                           *
 *  Audio Communication Group                                                        *
 *  www.ak.tu-berlin.de                                                              *
 *  Copyright 2006-2008                                                              *
 *                                                                                   *
 *                                                                                   *
 *  This program is free software; you can redistribute it and/or modify             *
 *  it under the terms of the GNU General Public License as published by             *
 *  the Free Software Foundation; either version 2 of the License, or                *
 *  (at your option) any later version.                                              *
 *                                                                                   *
 *  This program is distributed in the hope that it will be useful,                  *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of                   *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                    *
 *  GNU General Public License for more details.                                     *
 *                                                                                   *
 *  You should have received a copy of the GNU General Public License                *
 *  along with this program; if not, write to the Free Software                      *
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.       *
 *                                                                                   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "impulseresponse.h"
#include "fwonder_config.h"

#include <iostream>
#include <cmath>


using namespace std;

ImpulseResponseChannel::ImpulseResponseChannel( size_t IRPartitionSize ) : IRPartitionSize( IRPartitionSize )
{
}


ImpulseResponseChannel::~ImpulseResponseChannel()
{
    while( ! empty() )
    {
        delete back();
        pop_back();
    }
}


FftwBuf& ImpulseResponseChannel::addBuffer()
{
    //FftwBuf* buffer = new FftwBuf( 2 * IRPartitionSize );
    //push_back( buffer );
    //return *buffer;
    
    push_back( new FftwBuf( 2 * IRPartitionSize ) );
    return *back();
}


ImpulseResponseChannel::ImpulseResponseChannel( const ImpulseResponseChannel& other )
{
    // just preventing copying
}


void /*ImpulseResponseChannel&*/ ImpulseResponseChannel::operator = ( const ImpulseResponseChannel& other )
{
    // just preventing copying
}






ImpulseResponse::ImpulseResponse( string fileName, size_t IRPartitionSize, size_t maxNoPartitions ) : xPos( 0 ), yPos( 0 ), IRPartitionSize( IRPartitionSize ), noPartitions( 0 ) 
{
    killed = true;
    inUse  = false;
    
    SndfileHandle file( fileName.c_str() );

    firstPart = 0;

    noChannels = file.channels();

    if( noChannels == 0 )
    {
        cerr << "ERROR! IR: " << fileName << " -> number of channels " << noChannels << endl;
	return;
    }

    for( int i = 0; i < noChannels; ++i )
	channels.push_back( new ImpulseResponseChannel( IRPartitionSize ) );

    if( fwonderConf->IRVerbose )
        cout << "IR: " << fileName << " -> number of channels " << noChannels << endl;
    
    float inputMultiChannelDeinterleaveBuffer[ noChannels * IRPartitionSize  ];

    bool stop = false;

    while( ! stop && ( noPartitions < ( int ) maxNoPartitions ) )
    {
        sf_count_t numRead = file.readf( inputMultiChannelDeinterleaveBuffer, IRPartitionSize );

	// XXX:if the next lines are commented out numRead will be zero-padded
	// until max_partitions is reached otherwise reading is stopped if EOF is reached
	//if( numRead == 0 ) {
	//    stop = true;
	//    break;
	//}
        
	if( numRead != IRPartitionSize )
        {
            if( fwonderConf->IRVerbose )
                cout << "IR: " << fileName << " -> read " << numRead << ": padding" << endl;

	    for( unsigned int i = noChannels * numRead; i < IRPartitionSize * noChannels; ++i )
		inputMultiChannelDeinterleaveBuffer[ i ] = 0.0;
	}


	for( int i = 0; i < noChannels; ++i )
        {
	    FftwBuf& newBuffer  = channels[ i ]->addBuffer();
	    float*   newSamples = newBuffer.getSamples();

	    // if this is the last partition then do a fade out
	    if( noPartitions == ( ( int ) maxNoPartitions - 1 ) ) 
            {
		float fadeFactor   = 1.0;
		float fadeStepSize = 1.0 / ( float ) IRPartitionSize;
		for( unsigned int j = 0; j < IRPartitionSize; ++j )
                {
		    newSamples[ j ] = inputMultiChannelDeinterleaveBuffer[ i + j * noChannels ] / ( float ) ( 2 * IRPartitionSize) * fadeFactor;
		    newSamples[ j + IRPartitionSize ] = 0.0;
		    fadeFactor -= fadeStepSize;
		}
	    }
            else
            {
                for( unsigned int j = 0; j < IRPartitionSize; ++j )
                {
		    newSamples[ j ] = inputMultiChannelDeinterleaveBuffer[ i + j * noChannels ] / ( float ) ( 2 * IRPartitionSize );
		    newSamples[ j + IRPartitionSize ] = 0.0;
		}
	    }

	    newBuffer.fft();
	}

	++noPartitions;
    }

    killed = false;
}


//XXX: check use of this constructor, most calls use fadeInPartition = -1 which prevents division by 0 in line 245 which would occurr because of the default value fadeInLength=0
//      this also prevents any use of the fwonderConf->tailWindow data which obviously should only be used when this is a tail IR, seems to be a HACK
ImpulseResponse::ImpulseResponse( string fileName, size_t IRPartitionSize, size_t maxNoPartitions, int fadeInPartition, int fadeInLength ) 
                                : xPos( 0 ), yPos( 0 ), IRPartitionSize( IRPartitionSize ), noPartitions( 0 )
{
    killed = true;
    inUse  = false;

    SndfileHandle file( fileName.c_str() );

    if( fadeInPartition < 0 )
	firstPart = 0;
    else
	firstPart = fadeInPartition;

    noChannels = file.channels();

    if( noChannels == 0 )
    {
        cerr << "ERROR! IR: " << fileName << " -> number of channels " << noChannels << endl;
	return;
    }

    for( int i = 0; i < noChannels; ++i )
	channels.push_back( new ImpulseResponseChannel( IRPartitionSize ) );

    if( fwonderConf->IRVerbose ) 
        cout << "IR: " << fileName << " -> number of channels " << noChannels << endl;
    
    float inputMultiChannelDeinterleaveBuffer[ noChannels * IRPartitionSize  ];

    bool stop = false;

    while( ! stop && ( noPartitions < ( int ) maxNoPartitions ) )
    {
	sf_count_t numRead = file.readf( inputMultiChannelDeinterleaveBuffer, IRPartitionSize );

	// XXX:if the next lines are commented out numRead will be zero-padded 
	//until max_partitions is reached otherwise reading is stopped if EOF is reached
	//if( numRead == 0 ) {
	  //stop = true;
	  //break;
	//}
	
	if( numRead != IRPartitionSize )
        {
            if( fwonderConf->IRVerbose ) 
                cout << "IR: " << fileName << " -> read " << numRead << ": padding" << endl;

	    for( unsigned int i = noChannels * numRead; i < IRPartitionSize * noChannels; ++i )
                inputMultiChannelDeinterleaveBuffer[ i ] = 0.0;
	}


	for( int i = 0; i < noChannels; ++i )
        {
	    FftwBuf& newBuffer  = channels[ i ]->addBuffer();
	    float*   newSamples = newBuffer.getSamples();

	    // if this is the last partition then do a fade out.
	    if( noPartitions == ( ( int ) maxNoPartitions - 1 ) )
            {
		float fadeFactor   =  1.0;
		float fadeStepSize = -1.0 / ( float ) IRPartitionSize;

		for( unsigned int j = 0; j < IRPartitionSize; ++j ) 
                {
		    if( fwonderConf->tailWindow == LINEAR ) 
                        newSamples[ j ] = inputMultiChannelDeinterleaveBuffer[ i + j * noChannels ] / ( float ) ( 2 * IRPartitionSize ) * fadeFactor;
                    else if (fwonderConf->tailWindow == NOWIN ) 
                        newSamples[ j ] = inputMultiChannelDeinterleaveBuffer[ i + j * noChannels ] / ( float ) ( 2 * IRPartitionSize );
                    else if ( fwonderConf->tailWindow == COS2 ) 
                        newSamples[ j ] = inputMultiChannelDeinterleaveBuffer[ i + j * noChannels ] / ( float ) ( 2 * IRPartitionSize ) * cos( M_PI * ( 1 - fadeFactor ) / 2 );

		    newSamples[ j + IRPartitionSize ] = 0.0;

		    fadeFactor += fadeStepSize;
		}

	    }
            else if( noPartitions < fadeInPartition )
            {
                // set everything so zero for all partitions before the partition that should be used for fade in
		for( unsigned int j = 0; j < IRPartitionSize; ++j )
                {
		    newSamples[ j ]                   = 0.0;
		    newSamples[ j + IRPartitionSize ] = 0.0;
		}

	    }
            else if( noPartitions == fadeInPartition ) // this is the first partition, so do a fade in
            {
		float fadeFactor   = 0.0;
		float fadeStepSize = 1.0 / ( float ) fadeInLength; //XXX:possible divide by 0 

		for( unsigned int j = 0; j < IRPartitionSize; ++j )
                {
		    if( fwonderConf->tailWindow == LINEAR )
                        newSamples[ j ] = inputMultiChannelDeinterleaveBuffer[ i + j * noChannels ] / ( float ) ( 2 * IRPartitionSize ) * fadeFactor;
		    else if( fwonderConf->tailWindow == NOWIN )
			newSamples[ j ] = inputMultiChannelDeinterleaveBuffer[ i + j * noChannels ] / ( float ) ( 2 * IRPartitionSize );
		    else if( fwonderConf->tailWindow == COS2 )
			newSamples[ j ] = inputMultiChannelDeinterleaveBuffer[ i + j * noChannels ] / ( float ) ( 2 * IRPartitionSize ) * cos( M_PI * ( 1 - fadeFactor ) / 2 );

		    newSamples[ j + IRPartitionSize ] = 0.0;

		    fadeFactor += fadeStepSize;

		    if( fadeFactor >= 1.0 )
                        fadeFactor = 1.0;
		}
	    } 
            else
            {
		for( unsigned int j = 0; j < IRPartitionSize; ++j )
                {
		    newSamples[ j ] = inputMultiChannelDeinterleaveBuffer[ i + j * noChannels ] / ( float ) ( 2 * IRPartitionSize );
		    newSamples[ j + IRPartitionSize ] = 0.0;
		}
	    }

	    newBuffer.fft();
	}

	++noPartitions;

    }

    //if( fwonderConf->IRVerbose )
        //cout << "IR: " << fileName << " -> partition size: " << IRPartitionSize << ", parts: " << noPartitions << "   first part: " << firstPart << endl;

    killed = false; 
}


//// Create Empty impulseresponse.
//ImpulseResponse::ImpulseResponse( size_t IRPartitionSize, size_t maxNoPartitions, int noChannels ) : xPos( 0 ), yPos( 0 ), IRPartitionSize( IRPartitionSize ), noChannels( noChannels ), noPartitions( 0 )
//{
//    killed = false;
//
//    for( int i = 0; i < noChannels; ++i )
//	channels.push_back( new ImpulseResponseChannel( IRPartitionSize ) );
//
//    for( unsigned int i = 0; i < maxNoPartitions; ++i )
//    {
//        //for( int j = 0; j < noChannels; ++j )
//        //{
//	//  //FftwBuf *newBuffer = channels[j]->new_buf( 0 );
//	//}
//	++noPartitions;
//    }
//}


ImpulseResponse::~ImpulseResponse() 
{
    killed = true;

    while( ! channels.empty() )
    {
        delete channels.back();
        channels.pop_back();
    }
}


int ImpulseResponse::getNoChannels()
{
    return noChannels; 
}


int ImpulseResponse::getNoPartitions()
{
    return noPartitions;
}


int ImpulseResponse::getFirstPartition() 
{
    return firstPart;
}


size_t ImpulseResponse::getPartitionsize()
{
    return IRPartitionSize;
}


ImpulseResponseChannel& ImpulseResponse::getChannel( int i )
{
    if( killed )
        cerr << "Error: ImpulseResponse::getChannel( " << i << " ) called on a killed IR!  " << endl;

    //if( ! inUse )
        //cerr << "Error: ImpulseResponse::getChannel( " << i << " ) called on an IR, that should not be in use! " << endl;

    // let this provoke an exception on the next call to channels.at()
    if( channels.empty() )
        cerr << "Error: ImpulseResponse::getChannel( " << i << " ) called with no channel available!  " << endl;

    //ImpulseResponseChannel& temp = *( channels.at( i ) ) ;

    //return temp;
    return *( channels.at( i ) );
}


ImpulseResponse::ImpulseResponse( const ImpulseResponse& other )
{
    // just preventing copying
}


void /*ImpulseResponse&*/ ImpulseResponse::operator = ( const ImpulseResponse& other )
{
    // just preventing copying
}
