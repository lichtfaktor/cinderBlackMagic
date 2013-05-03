//
//  BlackMagicCamera.cpp
//  V0.3
//
//  Created by Jens Heinen on 02.10.11.
//  Copyright 2011 Lichtfaktor. All rights reserved.
//

//#include <iostream>
#include "BlackMagicCamera.h"


BlackMagicCamera::BlackMagicCamera()
:  selectedDevice(NULL), deckLinkInput(NULL), 
screenPreviewCallback(NULL), supportFormatDetection(false), currentlyCapturing(false)
{
    currentlyCapturing = false;
    mCurrentDeviceIndex =-1;
    mInputMode=INPUTMODE_10YUV;
}


BlackMagicCamera::~BlackMagicCamera()
{
    if (currentlyCapturing) stopCapture();
	vector<IDeckLink*>::iterator it;
	
	// Release screen preview
	if (screenPreviewCallback != NULL)
	{
		screenPreviewCallback->Release();
		screenPreviewCallback = NULL;
	}
    
	// Release the IDeckLink list
	for(it = deviceList.begin(); it != deviceList.end(); it++)
	{
		(*it)->Release();
	}
}





bool BlackMagicCamera::setup(int device, int videomode,InputModes conversionMode){
    IDeckLinkIterator*	deckLinkIterator = NULL;
	IDeckLink*			deckLink = NULL;
	bool				result = false;
	
    mInputMode=conversionMode;
    
    mPixelThreshold = 0.5f; //TODO
    
    
	// Create an iterator
	deckLinkIterator = CreateDeckLinkIteratorInstance();
	if (deckLinkIterator == NULL)
	{
		console() << "This application requires the Desktop Video drivers installed. Please install the Blackmagic Desktop Video drivers to use the features of this application." << endl;
		goto bail;
	}
	
	// List all DeckLink devices
	while (deckLinkIterator->Next(&deckLink) == S_OK)
	{
		// Add device to the device list
		deviceList.push_back(deckLink);
	}
	
	if (deviceList.size() == 0)
	{
		console() << "You will not be able to use the features of this application until a Blackmagic device is installed. This application requires at least one Blackmagic device." << endl;
		goto bail;
	}
    
	result = true;
    mCurrentDeviceIndex=-1;
    
    getInputList();
    selectDevice(device,videomode,conversionMode);
	
bail:
	if (deckLinkIterator != NULL)
	{
		deckLinkIterator->Release();
		deckLinkIterator = NULL;
	}
	
	return result;
};                        //general setup 

vector<string> BlackMagicCamera::getInputList(){
    vector<string>		nameList;
	int					deviceIndex = 0;
	
    nameList.clear();
    console() << "Blackmagic Devices found" << endl;

    int i=0;
	while (deviceIndex < deviceList.size())
	{		
		CFStringRef	cfStrName;
        string name;
		
		// Get the name of this device
		if (deviceList[deviceIndex]->GetDisplayName(&cfStrName) == S_OK)
		{		
            name=convertCfString(cfStrName);
			nameList.push_back(name);
        }
		else
		{
            name = "DeckLink";
            nameList.push_back(name);
		}
        console() << "[" << toString(i) << "]Device: " << name << endl;
        i++;
		deviceIndex++;
	}
	
	return nameList;
};                 //returns the names of the inputs

bool BlackMagicCamera::selectDevice(int deviceID, int videoModeIndex, InputModes inputMode )
{
	//IDeckLinkAttributes*			deckLinkAttributes = NULL;
	IDeckLinkDisplayModeIterator*	displayModeIterator = NULL;
	IDeckLinkDisplayMode*			displayMode = NULL;
    IDeckLinkDisplayMode*			selectedDisplayMode = NULL;
    //IDeckLinkDisplayMode*			selectedOutputDisplayMode = NULL;
	bool							result = false;
    
	// Check device index
	if (deviceID >= deviceList.size())
	{
		console() << "This application was unable to select the device. Error getting selecting the DeckLink device." << endl;
		goto bail;
	}
	
	// A new device has been selected.
	// Release the previous selected device and mode list
	if (deckLinkInput != NULL){
		deckLinkInput->Release();
        //deckLinkInput == NULL;
    }
	// Get the IDeckLinkInput Device 
	if ((deviceList[deviceID]->QueryInterface(IID_IDeckLinkInput, (void**)&deckLinkInput) != S_OK))
	{
		console() << "This application was unable to obtain IDeckLinkInput for the selected device. Error getting setting up capture." << endl;
		deckLinkInput = NULL;
		goto bail;
	}
    mCurrentDeviceIndex=deviceID;
    
	
	// retrieve and print video modes
	if (deckLinkInput->GetDisplayModeIterator(&displayModeIterator) == S_OK)
	{
        int i=0;
        CFStringRef	modeName;
		while (displayModeIterator->Next(&displayMode) == S_OK){
            string name;
            if (displayMode->GetName(&modeName) == S_OK){
                name=convertCfString(modeName);
            }else {
                name="Unknown mode";
            }
            console() << "[" << toString(i) << "]InputMode: " << name << endl;
            if (i==videoModeIndex) selectedDisplayMode=displayMode;
            i++;
        }
		displayModeIterator->Release();
	}
    
    //check if we selected a valid video mode
    if (selectedDisplayMode == NULL)
	{
		console() << "An invalid display mode was selected. Error starting the capture" << endl;
		return false;
	}
	
	// Set capture callback
	deckLinkInput->SetCallback(this);
	
	// Set the video input mode
    HRESULT hr;
    //bmdVideoInputFlagDefault
    _BMDPixelFormat pixelFormat;
    if (inputMode==INPUTMODE_10RGB){
        pixelFormat = bmdFormat10BitRGB;
    }else if (inputMode==INPUTMODE_10YUV || inputMode==INPUTMODE_10YUVTORGB){
        pixelFormat =bmdFormat10BitYUV;
    }else{
        pixelFormat = bmdFormat8BitYUV;
    }
    
    
	if ( (hr=deckLinkInput->EnableVideoInput(selectedDisplayMode->GetDisplayMode(), pixelFormat, 0)) != S_OK)
	{
        if (hr==E_INVALIDARG) console() << "E_INVALIDARG" << endl;
        if (hr==E_ACCESSDENIED) console() << "E_ACCESSDENIED" << endl;
        if (hr==E_OUTOFMEMORY) console() << "E_OUTOFMEMORY" << endl;
        if (hr==E_FAIL) console() << "E_FAIL" << endl;
		console() << "This application was unable to select the chosen video mode. Perhaps, the selected device is currently in-use. Error starting the capture " << hr << endl;
		return false;
	}
    
    mWidth=selectedDisplayMode->GetWidth();
    mHeight=selectedDisplayMode->GetHeight();
    mIsHD=false;
    if (mWidth>=1280 && mHeight>=720) mIsHD=true;
    
	result = true;
	
bail:
	return result;
}


bool BlackMagicCamera::startCapture()
{

	
	// Start the capture
	if (deckLinkInput->StartStreams() != S_OK)
	{
		console() << "This application was unable to start the capture. Perhaps, the selected device is currently in-use. Error starting the capture" << endl;
		return false;
	}
	
	currentlyCapturing = true;
	
	return true;
}

void BlackMagicCamera::stopCapture()
{
	// Stop the capture
	deckLinkInput->StopStreams();
	
	// Delete capture callback
	deckLinkInput->SetCallback(NULL);
	
	currentlyCapturing = false;
}

HRESULT	BlackMagicCamera::VideoInputFormatChanged ( BMDVideoInputFormatChangedEvents notificationEvents,  IDeckLinkDisplayMode *newMode,  BMDDetectedVideoInputFormatFlags detectedSignalFlags)
{	
	UInt32				modeIndex = 0;
	
//	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	
	// Restart capture with the new video mode if told to

		// Stop the capture
		deckLinkInput->StopStreams();
		
		// Set the video input mode
		if (deckLinkInput->EnableVideoInput(newMode->GetDisplayMode(), bmdFormat8BitYUV, bmdVideoInputEnableFormatDetection) != S_OK)
		{
			stopCapture();
			console() << "This application was unable to select the new video mode. Error restarting the capture." << endl;
			goto bail;
		}
		
		// Start the capture
		if (deckLinkInput->StartStreams() != S_OK)
		{
			stopCapture();
			console() << "This application was unable to start the capture on the selected device. Error restarting the capture." << endl;

			goto bail;
		}		
	
	
	// Find the index of the new mode in the mode list so we can update the UI
	while (modeIndex < modeList.size()) {
		if (modeList[modeIndex]->GetDisplayMode() == newMode->GetDisplayMode())
		{
			break;
		}
		modeIndex++;
	}
	startCapture();
    
bail:
	return S_OK;
}

HRESULT BlackMagicCamera::VideoInputFrameArrived (/* in */ IDeckLinkVideoInputFrame* videoFrame, /* in */ IDeckLinkAudioInputPacket* audioPacket)
{
    //console() << " newFrame " << mNewFrameAvailable << endl;

    //bool					hasValidInputSource = (videoFrame->GetFlags() & bmdFrameHasNoInputSource) != 0 ? false : true;
	/* TODO this is buggy
    AncillaryDataStruct		ancillaryData;
	
	
	// Get the various timecodes and userbits for this frame
	getAncillaryDataFromFrame(videoFrame, bmdTimecodeVITC, &ancillaryData.vitcF1Timecode, &ancillaryData.vitcF1UserBits);
	getAncillaryDataFromFrame(videoFrame, bmdTimecodeVITCField2,&ancillaryData.vitcF2Timecode, &ancillaryData.vitcF2UserBits);
	getAncillaryDataFromFrame(videoFrame, bmdTimecodeRP188VITC1,&ancillaryData.rp188vitc1Timecode, &ancillaryData.rp188vitc1UserBits);
	getAncillaryDataFromFrame(videoFrame, bmdTimecodeRP188LTC, &ancillaryData.rp188ltcTimecode, &ancillaryData.rp188ltcUserBits);
	getAncillaryDataFromFrame(videoFrame, bmdTimecodeRP188VITC2, &ancillaryData.rp188vitc2Timecode, &ancillaryData.rp188vitc2UserBits);
	*/
    
        
	// create surface if needed
    
    Surface32f mSurface32;
    Surface mSurface;
    if (mInputMode==INPUTMODE_10YUV || mInputMode==INPUTMODE_10YUVTORGB || mInputMode==INPUTMODE_10RGB){
        //create a surface
       mSurface32 = Surface32f(videoFrame->GetWidth(),videoFrame->GetHeight(),true, SurfaceChannelOrder::RGB);
    }else{
        //create a surface
        mSurface = Surface8u(videoFrame->GetWidth(),videoFrame->GetHeight(),true, SurfaceChannelOrder::RGB);
    }
    
    // FRAME ANALYSIS
    Vec2f pixelCenter=Vec2f(0,0);
    int pixelCount=0;
    
    switch (mInputMode){
        case INPUTMODE_8YUV:
        {
            int32_t srcPitch=videoFrame->GetRowBytes();
            Byte * srcPointer;
            Byte * srcPointerRow;
            videoFrame->GetBytes((void**)&srcPointer);
            
            Surface8u::Iter iter = mSurface.getIter( mSurface.getBounds() );
            int y=0;
            Byte pixelThreshold=(Byte)(mPixelThreshold*255.0f);
            while( iter.line() ) {
                srcPointerRow = srcPointer;
                int x=0;
                while( iter.pixel() ) {
                    
                    Byte cb=*srcPointer;
                    srcPointer++;
                    Byte  y1=*srcPointer;
                    srcPointer++;
                    Byte  cr=*srcPointer;
                    srcPointer++;
                    Byte  y2=*srcPointer;
                    srcPointer++;
                    
                    iter.r() = y1;
                    iter.g() = cr;
                    iter.b() = cb;
                    if (y1>=pixelThreshold){
                        pixelCenter+=Vec2f(x,y);
                        pixelCount++;
                    }
                    
                    x++;
                    iter.pixel();
                    iter.r() = y2;
                    iter.g() = cr;
                    iter.b() = cb;
                    if (y2>=pixelThreshold){
                        pixelCenter+=Vec2f(x,y);
                        pixelCount++;
                    }
                    
                    x++;
                }
                y++;
                srcPointer = srcPointerRow+srcPitch;
                
            }
            break;
        }
        case INPUTMODE_8YUVTORGB:
        {
            int32_t srcPitch=videoFrame->GetRowBytes();
            Byte * srcPointer;
            Byte * srcPointerRow;
            videoFrame->GetBytes((void**)&srcPointer);
            
            Byte pixelThreshold=(Byte)(mPixelThreshold*255.0f);

            Surface8u::Iter iter = mSurface.getIter( mSurface.getBounds() );
            int y=0;
            while( iter.line() ) {
                srcPointerRow = srcPointer;
                int x=0;
                while( iter.pixel() ) {
                    
                    Byte cb=*srcPointer;
                    srcPointer++;
                    Byte  y1=*srcPointer;
                    srcPointer++;
                    Byte  cr=*srcPointer;
                    srcPointer++;
                    Byte  y2=*srcPointer;
                    srcPointer++;
                    
                    Vec3i col=fromYUVToRGB(y1,cr,cb);
                    
                    iter.r() = col.x;
                    iter.g() = col.y;
                    iter.b() = col.z;
                    if (y1>=pixelThreshold){
                        pixelCenter+=Vec2f(x,y);
                        pixelCount++;
                    }
                    
                    iter.pixel();
                    x++;
                    
                    col=fromYUVToRGB(y2,cr,cb);
                    iter.r() = col.x;
                    iter.g() = col.y;
                    iter.b() = col.z;
                    if (y2>=pixelThreshold){
                        pixelCenter+=Vec2f(x,y);
                        pixelCount++;
                    }
                    x++;
                }
                y++;
                srcPointer = srcPointerRow+srcPitch;
                
            }
            break;
        }
        case INPUTMODE_10YUV:
        {
            const uint32 mask=1023;
            
            uint32_t srcPitch=videoFrame->GetRowBytes();
            uint32 * srcPointer;
            uint32 * srcPointerRow;
            videoFrame->GetBytes((void**)&srcPointer);
                           
            Surface32f::Iter iter = mSurface32.getIter( mSurface32.getBounds() );
             
            int y=0;
            uint32 pixelThreshold=(int)(mPixelThreshold*1023.0f);
            while( iter.line() ) {
                srcPointerRow = srcPointer;
                int x=0;
                while( iter.pixel() ) {
                    
                    uint32 int0=*srcPointer;
                    srcPointer++;
                    uint32  int1=*srcPointer;
                    srcPointer++;
                    uint32  int2=*srcPointer;
                    srcPointer++;
                    uint32  int3=*srcPointer;
                    srcPointer++;
                    
                    uint32 cr0=(int0 >> 20) & mask;
                    uint32  y0=(int0 >> 10) & mask;
                    uint32  cb0=int0 & mask;
                    uint32  y2=(int1 >> 20) & mask;
                    uint32  cb2=(int1 >> 10) & mask;
                    uint32  y1=int1 & mask;
                    uint32  cb4=(int2 >> 20) & mask;
                    uint32  y3=(int2 >> 10) & mask;
                    uint32  cr2=int2 & mask;
                    uint32  y5=(int3 >> 20) & mask;
                    uint32  cr4=(int3 >> 10) & mask;
                    uint32  y4=int3 & mask;
                    
                     iter.r() = (float)(y0)/1023.0f;
                     iter.g() = (float)(cr0)/1023.0f;
                     iter.b() = (float)(cb0)/1023.0f;
                    if (y0>=pixelThreshold){
                        pixelCenter+=Vec2f(x,y);
                        pixelCount++;
                    }
                    x++;
                     iter.pixel();
                     
                     iter.r() = (float)(y1)/1023.0f;
                     iter.g() = (float)(cr0)/1023.0f;
                     iter.b() = (float)(cb0)/1023.0f;
                    if (y1>=pixelThreshold){
                        pixelCenter+=Vec2f(x,y);
                        pixelCount++;
                    }
                    x++;
                     iter.pixel();
                     iter.r() = (float)(y2)/1023.0f;
                     iter.g() = (float)(cr2)/1023.0f;
                     iter.b() = (float)(cb2)/1023.0f;
                    if (y2>=pixelThreshold){
                        pixelCenter+=Vec2f(x,y);
                        pixelCount++;
                    }
                    x++;
                     iter.pixel();
                     iter.r() = (float)(y3)/1023.0f;
                     iter.g() = (float)(cr2)/1023.0f;
                     iter.b() = (float)(cb2)/1023.0f;
                    if (y3>=pixelThreshold){
                        pixelCenter+=Vec2f(x,y);
                        pixelCount++;
                    }
                    x++;
                     iter.pixel();
                     iter.r() = (float)(y4)/1023.0f;
                     iter.g() = (float)(cr4)/1023.0f;
                     iter.b() = (float)(cb4)/1023.0f;
                    if (y4>=pixelThreshold){
                        pixelCenter+=Vec2f(x,y);
                        pixelCount++;
                    }
                    x++;
                     iter.pixel();
                     iter.r() = (float)(y5)/1023.0f;
                     iter.g() = (float)(cr4)/1023.0f;
                     iter.b() = (float)(cb4)/1023.0f;
                    if (y5>=pixelThreshold){
                        pixelCenter+=Vec2f(x,y);
                        pixelCount++;
                    }
                    x++;
                    
                }
                y++;
                srcPointer = srcPointerRow+srcPitch/4;      //pitch is 8bit based but pointer is 32bit
                
            }
            break;
        }
        case INPUTMODE_10RGB:
        {
            const uint32 mask=1023;
            
            uint32_t srcPitch=videoFrame->GetRowBytes();
            uint32 * srcPointer;
            uint32 * srcPointerRow;
            videoFrame->GetBytes((void**)&srcPointer);
            
            Surface32f::Iter iter = mSurface32.getIter( mSurface32.getBounds() );
            
            int y=0;
            float luma;
            uint32 pixelThreshold=(int)(mPixelThreshold*1023.0f);
            while( iter.line() ) {
                srcPointerRow = srcPointer;
                int x=0;
                while( iter.pixel() ) {
                    
                    uint32 int0=*srcPointer;
                    srcPointer++;
                    uint32  int1=*srcPointer;
                    srcPointer++;
                    uint32  int2=*srcPointer;
                    srcPointer++;
                    uint32  int3=*srcPointer;
                    srcPointer++;
                    
                    uint32 cr0=(int0 >> 20) & mask;
                    uint32  y0=(int0 >> 10) & mask;
                    uint32  cb0=int0 & mask;
                    uint32  y2=(int1 >> 20) & mask;
                    uint32  cb2=(int1 >> 10) & mask;
                    uint32  y1=int1 & mask;
                    uint32  cb4=(int2 >> 20) & mask;
                    uint32  y3=(int2 >> 10) & mask;
                    uint32  cr2=int2 & mask;
                    uint32  y5=(int3 >> 20) & mask;
                    uint32  cr4=(int3 >> 10) & mask;
                    uint32  y4=int3 & mask;
                    
                    iter.r() = (float)(y0)/1023.0f;
                    iter.g() = (float)(cr0)/1023.0f;
                    iter.b() = (float)(cb0)/1023.0f;
                    luma = (iter.r() +iter.g()+iter.b())/3.0f;
                    if (luma>=pixelThreshold){      //sound analysis
                        pixelCenter+=Vec2f(x,y);
                        pixelCount++;
                    }
                    x++;
                    iter.pixel();
                    
                    iter.r() = (float)(y1)/1023.0f;
                    iter.g() = (float)(cr0)/1023.0f;
                    iter.b() = (float)(cb0)/1023.0f;
                    luma = (iter.r() +iter.g()+iter.b())/3.0f;
                    if (luma>=pixelThreshold){
                        pixelCenter+=Vec2f(x,y);
                        pixelCount++;
                    }
                    x++;
                    iter.pixel();
                    iter.r() = (float)(y2)/1023.0f;
                    iter.g() = (float)(cr2)/1023.0f;
                    iter.b() = (float)(cb2)/1023.0f;
                    luma = (iter.r() +iter.g()+iter.b())/3.0f;
                    if (luma>=pixelThreshold){
                        pixelCenter+=Vec2f(x,y);
                        pixelCount++;
                    }
                    x++;
                    iter.pixel();
                    iter.r() = (float)(y3)/1023.0f;
                    iter.g() = (float)(cr2)/1023.0f;
                    iter.b() = (float)(cb2)/1023.0f;
                    luma = (iter.r() +iter.g()+iter.b())/3.0f;
                    if (luma>=pixelThreshold){
                        pixelCenter+=Vec2f(x,y);
                        pixelCount++;
                    }
                    x++;
                    iter.pixel();
                    iter.r() = (float)(y4)/1023.0f;
                    iter.g() = (float)(cr4)/1023.0f;
                    iter.b() = (float)(cb4)/1023.0f;
                    luma = (iter.r() +iter.g()+iter.b())/3.0f;
                    if (luma>=pixelThreshold){
                       pixelCenter+=Vec2f(x,y);
                        pixelCount++;
                    }
                    x++;
                    iter.pixel();
                    iter.r() = (float)(y5)/1023.0f;
                    iter.g() = (float)(cr4)/1023.0f;
                    iter.b() = (float)(cb4)/1023.0f;
                    luma = (iter.r() +iter.g()+iter.b())/3.0f;
                    if (luma>=pixelThreshold){
                        pixelCenter+=Vec2f(x,y);
                        pixelCount++;
                    }
                    x++;
                    
                }
                y++;
                srcPointer = srcPointerRow+srcPitch/4;      //pitch is 8bit based but pointer is 32bit
                
            }
            break;
        }
        case INPUTMODE_10YUVTORGB:
            {
                const uint32 mask=1023;
                
                uint32_t srcPitch=videoFrame->GetRowBytes();
                uint32 * srcPointer;
                uint32 * srcPointerRow;
                videoFrame->GetBytes((void**)&srcPointer);
                
                Surface32f::Iter iter = mSurface32.getIter( mSurface32.getBounds() );
                
                int y=0;
                uint32 pixelThreshold=(int)(mPixelThreshold*1023.0f);
                while( iter.line() ) {
                    srcPointerRow = srcPointer;
                    int x=0;
                    while( iter.pixel() ) {
                        
                        uint32 int0=*srcPointer;
                        srcPointer++;
                        uint32  int1=*srcPointer;
                        srcPointer++;
                        uint32  int2=*srcPointer;
                        srcPointer++;
                        uint32  int3=*srcPointer;
                        srcPointer++;
                        
                        uint32 cr0=(int0 >> 20) & mask;
                        uint32  y0=(int0 >> 10) & mask;
                        uint32  cb0=int0 & mask;
                        uint32  y2=(int1 >> 20) & mask;
                        uint32  cb2=(int1 >> 10) & mask;
                        uint32  y1=int1 & mask;
                        uint32  cb4=(int2 >> 20) & mask;
                        uint32  y3=(int2 >> 10) & mask;
                        uint32  cr2=int2 & mask;
                        uint32  y5=(int3 >> 20) & mask;
                        uint32  cr4=(int3 >> 10) & mask;
                        uint32  y4=int3 & mask;
                        
                        Vec3f col=fromYUV10ToRGB(y0,cr0,cb0);
                        iter.r() = col.x;
                        iter.g() = col.y;
                        iter.b() = col.z;
                        if (y0>=pixelThreshold){
                            pixelCenter+=Vec2f(x,y);
                            pixelCount++;
                        }
                        x++;
                        iter.pixel();
                        
                        col=fromYUV10ToRGB(y1,cr0,cb0);
                        iter.r() = col.x;
                        iter.g() = col.y;
                        iter.b() = col.z;
                        if (y1>=pixelThreshold){
                            pixelCenter+=Vec2f(x,y);
                            pixelCount++;
                        }
                        x++;
                        iter.pixel();
                        col=fromYUV10ToRGB(y2,cr2,cb2);
                        iter.r() = col.x;
                        iter.g() = col.y;
                        iter.b() = col.z;
                        if (y2>=pixelThreshold){
                            pixelCenter+=Vec2f(x,y);
                            pixelCount++;
                        }
                        x++;
                        iter.pixel();
                        col=fromYUV10ToRGB(y3,cr2,cb2);
                        iter.r() = col.x;
                        iter.g() = col.y;
                        iter.b() = col.z;
                        if (y3>=pixelThreshold){
                            pixelCenter+=Vec2f(x,y);
                            pixelCount++;
                        }
                        x++;
                        iter.pixel();
                        col=fromYUV10ToRGB(y4,cr4,cb4);
                        iter.r() = col.x;
                        iter.g() = col.y;
                        iter.b() = col.z;
                        if (y4>=pixelThreshold){
                            pixelCenter+=Vec2f(x,y);
                            pixelCount++;
                        }
                        x++;
                        iter.pixel();
                        col=fromYUV10ToRGB(y5,cr4,cb4);
                        iter.r() = col.x;
                        iter.g() = col.y;
                        iter.b() = col.z;
                        if (y5>=pixelThreshold){
                            pixelCenter+=Vec2f(x,y);
                            pixelCount++;
                        }
                        x++;
                        //iter.pixel();
                        
                    }
                    y++;
                    srcPointer = srcPointerRow+srcPitch/4;      //pitch is 8bit based but pointer is 32bit
                    
                }
                break;
        }
    }   //end switch inputmode

    // FRAME ANALYSIS
    if (pixelCount==0){
        mPixelCenter = Vec2f(0.5f,0.5f);
        mPixelCount=0;
    }else{
        mPixelCenter = pixelCenter / Vec2f(mWidth,mHeight) / pixelCount;
        mPixelCount = pixelCount;
    }
    
    //save surface to buffer and delete frames if there are too many
    mFrameBufferMutex.lock();
    if (mInputMode==INPUTMODE_10YUV || mInputMode==INPUTMODE_10YUVTORGB || mInputMode==INPUTMODE_10RGB){
        
        
        if (mSurface32Buffer.size()>3){
            mSurface32Buffer.erase(mSurface32Buffer.begin());
        }
        mSurface32Buffer.push_back(mSurface32);
        
        //if (mSurface32Buffer.size()>1) console() << mSurface32Buffer.size() << " frames available" << endl;
    }else{
        if (mSurfaceBuffer.size()>3){
            mSurfaceBuffer.erase(mSurfaceBuffer.begin());
        }
        mSurfaceBuffer.push_back(mSurface);

        //if (mSurfaceBuffer.size()>1) console() << mSurfaceBuffer.size() << " frames available" << endl;
    }
    mFrameBufferMutex.unlock();
    

    //check if a new frame is already there - if this happens the main app is too slow and will skip a frame
    // we could save all frames to an array to avoid skipped frames
    // best would be to just copy full frames
    

	return S_OK;
}

Vec3i BlackMagicCamera::fromYUVToRGB(Byte cy, Byte ccr, Byte ccb){
    const float y_const = 0.0625;
    const float vu_const = 0.5;
    
    //convert hd yuv to rgb HD
    float y=float(cy)/255.0f;       //y
    float cr=float(ccr)/255.0f;       //u = cr
    float cb=float(ccb)/255.0f;       //v = cb
    
    float r = (1.164 * (y - y_const)) + (1.793 * (cr - vu_const));
    float g = (1.164 * (y - y_const)) - (0.534 * (cr - vu_const)) - (0.213 * (cb - vu_const));
    float b = (1.164 * (y - y_const)) + (2.115 * (cb - vu_const));
    
    r=MAX(MIN(r,1.0),0.0);
    g=MAX(MIN(g,1.0),0.0);
    b=MAX(MIN(b,1.0),0.0);
    return Vec3i(int(r*255.0f),int(g*255.0f),int(b*255.0f));
}
Vec3f BlackMagicCamera::fromYUV10ToRGB(uint32 cy, uint32 ccr, uint32 ccb){
    
    
    // works good with bmctolnf (0..1) lnf(-8,8,0.0039)
     const float y_const = 0.0625f;
    const float vu_const = 0.5f;

    //convert hd yuv to rgb HD
    float y=float(cy)/1023.0f;       //y
    float cr=float(ccr)/1023.0f;       //u = cr
    float cb=float(ccb)/1023.0f;       //v = cb
    
    float r = (1.164f * (y - y_const)) + (1.793f * (cr - vu_const));
    float g = (1.164f * (y - y_const)) - (0.534f * (cr - vu_const)) - (0.213f * (cb - vu_const));
    float b = (1.164f * (y - y_const)) + (2.115f * (cb - vu_const));
     
    
    /* war ganz gut mit bmc,bmc5 mit threshold oder bm3 ohne (aber noch deitliche lücken!)
    const float vu_const = 0.5f;

    //convert hd yuv to rgb HD
    float y=float(cy)/1023.0f;       //y
    float cr=float(ccr)/1023.0f;       //u = cr
    float cb=float(ccb)/1023.0f;       //v = cb
    
    float r = y + 1.402f * (cr - vu_const);
    float g = y - 0.344f * (cb - vu_const) - 0.714f * (cr - vu_const);
    float b = y  + 1.772 * (cb - vu_const);*/
    
    
    
    // works the best with bmc or bmc5 and threshold
    /*const float vu_const = 0.5f;
    
    //convert hd yuv to rgb HD
    float y=float(cy)/1023.0f;       //y
    float cr=float(ccr)/1023.0f;       //u = cr
    float cb=float(ccb)/1023.0f;       //v = cb
    
    float r = y + 1.13983f * (cr - vu_const);
    float g = y - 0.39465f * (cb - vu_const) - 0.58060f * (cr - vu_const);
    float b = y  + 2.03211 * (cb - vu_const);*/

    
   /* does not look better than above
    const float y_const = 0.0625f;
    const float vu_const = 0.5f;
    
    //convert hd yuv to rgb HD
    float y=float(cy)/1023.0f;       //y
    float cr=float(ccr)/1023.0f;       //u = cr
    float cb=float(ccb)/1023.0f;       //v = cb
    
    float r = (1.164f * (y - y_const)) + (1.596f * (cr - vu_const));
    float g = (1.164f * (y - y_const)) - (0.813f * (cr - vu_const)) - (0.391f * (cb - vu_const));
    float b = (1.164f * (y - y_const)) + (2.018f * (cb - vu_const));*/

    r=MAX(MIN(r,1.0f),0.0f);
    g=MAX(MIN(g,1.0f),0.0f);
    b=MAX(MIN(b,1.0f),0.0f);
    return Vec3f(r,g,b);
}

Vec3f BlackMagicCamera::fromYUVToRGB(float y, float cr, float cb){
    const float y_const = 0.0625;
    const float vu_const = 0.5;
    
    
    float r = (1.164 * (y - y_const)) + (1.793 * (cr - vu_const));
    float g = (1.164 * (y - y_const)) - (0.534 * (cr - vu_const)) - (0.213 * (cb - vu_const));
    float b = (1.164 * (y - y_const)) + (2.115 * (cb - vu_const));
    
    r=MAX(MIN(r,1.0),0.0);
    g=MAX(MIN(g,1.0),0.0);
    b=MAX(MIN(b,1.0),0.0);
    return Vec3f(r,g,b);
}

void BlackMagicCamera::getAncillaryDataFromFrame(IDeckLinkVideoInputFrame* videoFrame, BMDTimecodeFormat timecodeFormat, string *timecodeString, string *userBitsString)
{
	IDeckLinkTimecode*		timecode = NULL;
	CFStringRef				timecodeCFString;
	BMDTimecodeUserBits		userBits = 0;
	
	if ((videoFrame != NULL) && (timecodeString != NULL) && (userBitsString != NULL)
		&& (videoFrame->GetTimecode(timecodeFormat, &timecode) == S_OK))
	{
		if (timecode->GetString(&timecodeCFString) == S_OK)
		{
            
			*timecodeString = convertCfString(timecodeCFString);
			CFRelease(timecodeCFString);
		}
		else
		{
			*timecodeString = "";
		}
		
		timecode->GetTimecodeUserBits(&userBits);
		//TODO userBitsString = convertNsString(NSString(stringWithFormat:@"0x%08X", userBits));
		
		timecode->Release();
	}
	else
	{
		*timecodeString = "";
		*userBitsString = "";
	}
    
    
}

bool BlackMagicCamera::newFrameAvailable(){
    if (mInputMode==INPUTMODE_10YUV || mInputMode==INPUTMODE_10YUVTORGB  || mInputMode==INPUTMODE_10RGB){
        if (mSurface32Buffer.size()>0) return true;
    }else{
        if (mSurfaceBuffer.size()>0) return true;
    }
    return false;
};            //returns true id a new frame is available

Surface BlackMagicCamera::getFrame(){
    mFrameBufferMutex.lock();
    Surface surface=*mSurfaceBuffer.begin();       //save surface
    if (surface!=NULL){
        mSurfaceBuffer.erase(mSurfaceBuffer.begin());   //erase it from buffer
    }
    mFrameBufferMutex.unlock();
    return surface;
};
Surface32f BlackMagicCamera::getFrame32(){
    mFrameBufferMutex.lock();
    Surface32f surface=*mSurface32Buffer.begin();       //save surface
    if (surface!=NULL){
        mSurface32Buffer.erase(mSurface32Buffer.begin());   //erase it from buffer
    }
    mFrameBufferMutex.unlock();
    return surface;
};