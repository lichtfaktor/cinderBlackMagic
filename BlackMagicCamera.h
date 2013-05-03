//
//  BlackMagicCamera.h
//  V0.3
//
//  Created by Jens Heinen on 02.10.11.
//  Copyright 2011 Lichtfaktor. All rights reserved.
//

#ifndef Lichtmaler_BlackMagicCamera_h
#define Lichtmaler_BlackMagicCamera_h

#include <vector>
#include "cinder/app/AppBasic.h"
#include "cinder/Surface.h"
#include "cinder/CinderMath.h"
#include "cinder/Cocoa/CinderCocoa.h"
#include "cinder/Utilities.h"
#include "DeckLinkAPI.h"
#include "cinder/Thread.h"

using namespace std;
using namespace ci;
using namespace ci::app;
using namespace cinder::cocoa;


enum InputModes{
    INPUTMODE_8YUV=0,
    INPUTMODE_8YUVTORGB=1,
    INPUTMODE_10YUV=2,
    INPUTMODE_10YUVTORGB=3,
    INPUTMODE_10RGB=4           //currently not supported
};

typedef struct {
	// VITC timecodes and user bits for field 1 & 2
	string	vitcF1Timecode;
	string	vitcF1UserBits;
	string	vitcF2Timecode;
	string	vitcF2UserBits;
	
	// RP188 timecodes and user bits (VITC1, VITC2 and LTC)
	string	rp188vitc1Timecode;
	string	rp188vitc1UserBits;
	string	rp188vitc2Timecode;
	string	rp188vitc2UserBits;
	string	rp188ltcTimecode;
	string	rp188ltcUserBits;
} AncillaryDataStruct;

class BlackMagicCamera: public IDeckLinkInputCallback{
public:
    BlackMagicCamera();
	virtual ~BlackMagicCamera();
    bool            setup(int index, int videomode, InputModes conversionMode);                        //general setup
    vector<string>  getInputList();                 //returns the names of the inputs
    bool            selectDevice(int index, int videomode, InputModes inputMode);
    bool            startCapture();
    void            stopCapture();
    bool            newFrameAvailable();            //returns true id a new frame is available
    Surface         getFrame();
    Surface32f      getFrame32();
    
    int             getWidth(){
        return mWidth;
    };
    int             getHeight(){
        return mHeight;
    };
    uint            get3DLutTextureID();
    //
	// IDeckLinkInputCallback interface
	
	// IUnknown needs only a dummy implementation
	virtual HRESULT		QueryInterface (REFIID iid, LPVOID *ppv)	{return E_NOINTERFACE;}
	virtual ULONG		AddRef ()									{return 1;}
	virtual ULONG		Release ()									{return 1;}
    virtual HRESULT		VideoInputFormatChanged (/* in */ BMDVideoInputFormatChangedEvents notificationEvents, /* in */ IDeckLinkDisplayMode *newDisplayMode, /* in */ BMDDetectedVideoInputFormatFlags detectedSignalFlags);
	virtual HRESULT		VideoInputFrameArrived (/* in */ IDeckLinkVideoInputFrame* videoFrame, /* in */ IDeckLinkAudioInputPacket* audioPacket);	

    InputModes                      mInputMode;

    // FRAME ANALYSIS
    Vec2f                           mPixelCenter;
    int                             mPixelCount;
    
    // MUTEX
    mutex                         mFrameBufferMutex;
    
private:
    float                           mPixelThreshold;
    bool                            mIsHD;
    int                             mWidth;
    int                             mHeight;
    vector<Surface8u>               mSurfaceBuffer;               //the surface with the frame
    vector<Surface32f>              mSurface32Buffer;               //the surface with the frame
    std::vector<IDeckLink*>         deviceList;
	IDeckLink*                      selectedDevice;
	IDeckLinkInput*                 deckLinkInput;
//    IDeckLinkOutput*                deckLinkOutput;
    
    //IDeckLinkMutableVideoFrame *    m_rgbFrame;
    //IDeckLinkOutput *               m_deckLinkOutput;
	
	IDeckLinkScreenPreviewCallback*	screenPreviewCallback;
	std::vector<IDeckLinkDisplayMode*>	modeList;
	
	bool                            supportFormatDetection;
	bool                            currentlyCapturing;
    int                             mCurrentDeviceIndex;
    
    inline Vec3i                    fromYUVToRGB(Byte y, Byte cr, Byte cb);
    inline Vec3f                    fromYUV10ToRGB(uint32 cy, uint32 ccr, uint32 ccb);
    inline Vec3f                    fromYUVToRGB(float y, float cr, float cb);
    void                            getAncillaryDataFromFrame(IDeckLinkVideoInputFrame* videoFrame, BMDTimecodeFormat timecodeFormat, string* timecodeString, string* userBitsString);
    
};

#endif
