/*
 Copyright (C) 2004-2006 the Minisip Team
 
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
 
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.
 
 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

/* Copyright (C) 2004 
 *
 * Authors: Erik Eliasson <eliasson@it.kth.se>
 *          Johan Bilien <jobi@via.ecp.fr>
*/

#include<config.h>
#include<libmutil/dbg.h>
#include<libminisip/video/display/VideoDisplay.h>
#include<libminisip/video/VideoException.h>
#ifdef XV_SUPPORT
#include<libminisip/video/display/XvDisplay.h>
#endif
#ifdef SDL_SUPPORT
#include<libminisip/video/display/SdlDisplay.h>
#endif
#include<libminisip/video/display/X11Display.h>

#include<iostream>
#define NB_IMAGES 3

Mutex VideoDisplay::displayCounterLock;
uint32_t VideoDisplay::displayCounter = 0;


MRef<VideoDisplay *> VideoDisplay::create( uint32_t width, uint32_t height ){
        MRef<VideoDisplay *> display = NULL;
        
        VideoDisplay::displayCounterLock.lock();

#if defined SDL_SUPPORT || defined XV_SUPPORT
        if( VideoDisplay::displayCounter == 0 ){
                try{
#ifdef SDL_SUPPORT
                display = new SdlDisplay( width, height );
                display->start();
#elif defined XV_SUPPORT
                display =  new XvDisplay( width, height );
                display->start();
#endif
                displayCounter ++;
                displayCounterLock.unlock();
                return display;
                }
                catch( VideoException & exc ){
                        mdbg << "Error opening the video display: "
                             << exc.error() << end;
                }
        }
#endif

        try{
                display = new X11Display( width, height );
                display->start();
        }
        catch( VideoException & exc ){
                merr << "Error opening the video display: "
                        << exc.error() << end;
        }

        displayCounter ++;
        displayCounterLock.unlock();
        return display;
}

VideoDisplay::VideoDisplay(){
	show = false;
//	emptyImagesLock.lock();
        show = true;
}

VideoDisplay::~VideoDisplay(){
//        thread->join();
        VideoDisplay::displayCounterLock.lock();
        VideoDisplay::displayCounter --;
        VideoDisplay::displayCounterLock.unlock();

}

void VideoDisplay::start(){
        showWindow();
	thread = new Thread( this );        
}

void VideoDisplay::stop(){
	show = false;
	//FIXME
        filledImagesSem.inc();
}

void VideoDisplay::showWindow(){
        uint32_t i;
        MImage * mimage;

        createWindow();

        if( providesImage() ){
                for( i = 0; i < NB_IMAGES; i++ ){
                        mimage = allocateImage();
                        allocatedImages.push_back( mimage );
                        emptyImagesLock.lock();
                        emptyImages.push_back( mimage );
                        emptyImagesLock.unlock();
                        emptyImagesSem.inc();
                }
        }
}


/* The lock on emptyImages should always have been taken */
void VideoDisplay::hideWindow(){
        list<MImage *>::iterator i;

        
        while( ! emptyImages.empty() ){
                emptyImages.pop_front();
                emptyImagesSem.dec();
        }

        while( ! allocatedImages.empty() ){
                deallocateImage( *allocatedImages.begin() );
                allocatedImages.pop_front();
        }


        destroyWindow();
}


MImage * VideoDisplay::provideImage(){
        MImage * ret;


        // The decoder is running, wake the display if
        // it was sleeping
        showCondLock.lock();
        showCond.broadcast();
        showCondLock.unlock();

        emptyImagesSem.dec();
        emptyImagesLock.lock();
        /*
        emptyImagesLock.lock();
        if( emptyImages.empty() ){

                emptyImagesLock.unlock();

                emptyImagesCondLock.lock();
                emptyImagesCond.wait( &emptyImagesCondLock );
                emptyImagesCondLock.unlock();

                emptyImagesLock.lock();
        }

        */

        ret = *emptyImages.begin();
        emptyImages.pop_front();

        emptyImagesLock.unlock();

        return ret;
}


void VideoDisplay::run(){
        MImage * imageToDisplay;


        while( show ){

                handleEvents();

                if( !show ){
                        break;

                }
                filledImagesSem.dec();
                if( !show ){
                        break;

                }

                filledImagesLock.lock();

                imageToDisplay = *filledImages.begin();

                filledImages.pop_front();

                filledImagesLock.unlock();

                displayImage( imageToDisplay );


                if( providesImage() ){
                        emptyImagesLock.lock();

                        emptyImagesSem.inc();
                        emptyImages.push_back( imageToDisplay );

                        emptyImagesLock.unlock();
                }
        }

        emptyImagesLock.lock();

        filledImagesLock.lock();

        while( ! filledImages.empty() ){
                imageToDisplay = *filledImages.begin();

                filledImages.pop_front();

                displayImage( imageToDisplay );

        }

        filledImagesLock.unlock();

        hideWindow();

}

void VideoDisplay::handle( MImage * mimage ){

        filledImagesLock.lock();
        filledImages.push_back( mimage );
        filledImagesSem.inc();
        filledImagesLock.unlock();
}

bool VideoDisplay::providesImage(){
	return true;
}

void VideoDisplay::releaseImage( MImage * mimage ){
}

