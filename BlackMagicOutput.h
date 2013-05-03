//
//  BlackMagicOutput.h
//  Lichtmaler
//
//  Created by Jens Heinen on 08.10.11.
//  Copyright 2011 Lichtfaktor. All rights reserved.
//

#ifndef Lichtmaler_BlackMagicOutput_h
#define Lichtmaler_BlackMagicOutput_h

#include "cinder/gl/gl.h"
#include "cinder/gl/Fbo.h"
#include "cinder/app/AppBasic.h"
#include "cinder/Cocoa/CinderCocoa.h"
#include "cinder/Utilities.h"
#include "DeckLinkAPI.h"

using namespace std;
using namespace ci;
using namespace ci::app;
using namespace cinder::cocoa;

class BlackMagicOutput{
private:
	// DeckLink
	uint32_t					mWidth;
	uint32_t					mHeight;
	
	IDeckLink*					pDL;
	IDeckLinkOutput*			pDLOutput;
	IDeckLinkMutableVideoFrame*	pDLVideoFrame;
	
public:
	BlackMagicOutput();
	~BlackMagicOutput();
    
	bool setup(int deviceIndex, int outputIndex);
	void renderToDevice(gl::Fbo * outputFrameBuffer);
    
    int getWidth(){
        return mWidth;
    };
    
    int getHeight(){
        return mHeight;
    };
};

#endif
