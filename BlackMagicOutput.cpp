//
//  BlackMagicOutput.cpp
//  Lichtmaler
//
//  Created by Jens Heinen on 08.10.11.
//  Copyright 2011 Lichtfaktor. All rights reserved.
//

#include "BlackMagicOutput.h"

BlackMagicOutput::BlackMagicOutput()
: pDL(NULL), pDLOutput(NULL), pDLVideoFrame(NULL)
{
}

BlackMagicOutput::~BlackMagicOutput()
{
    
	if (pDLOutput != NULL)
	{   
        pDLOutput->DisableVideoOutput();
	}
    
	if (pDLVideoFrame != NULL)
	{
		pDLVideoFrame->Release();
		pDLVideoFrame = NULL;
	}
    
	if (pDLOutput != NULL)
	{
		pDLOutput->Release();
		pDLOutput = NULL;
	}		
	if (pDL != NULL)
	{
		pDL->Release();
		pDL = NULL;
	}		
	
}


bool BlackMagicOutput::setup(int deviceIndex,int outputIndex)
{
	bool bSuccess = FALSE;
	IDeckLinkIterator* pDLIterator = NULL;
    IDeckLink*	deckLink = NULL;
    IDeckLinkDisplayModeIterator*		pDLDisplayModeIterator;
	IDeckLinkDisplayMode*				pDLDisplayMode = NULL;
    IDeckLinkDisplayMode*               displayMode = NULL;

    int i=0;
    
	pDLIterator = CreateDeckLinkIteratorInstance();
	if (pDLIterator == NULL)
	{
		console() <<"This application requires the DeckLink drivers installed. Please install the Blackmagic DeckLink drivers to use the features of this application." << endl;
		goto error;
	}
    
    // List all DeckLink devices
    while (pDLIterator->Next(&deckLink) == S_OK)
    {
        CFStringRef	cfStrName;
        string name;
        
        // Get the name of this device
        if (deckLink->GetDisplayName(&cfStrName) == S_OK){		
            name=convertCfString(cfStrName);
        }else{
            name = "DeckLink";
        }
        
        console() << "[" << toString(i) << "]Device: " << name << endl;
        if (i==deviceIndex){
            pDL = deckLink;         //found the device
        }
        i++;
    }

	
	if (i == 0){
		console() << "You will not be able to use the features of this application until a Blackmagic device is installed. This application requires at least one Blackmagic device." << endl;
		goto error;
	}

    //get output device
	if (pDL->QueryInterface(IID_IDeckLinkOutput, (void**)&pDLOutput) != S_OK){
        console() << "Unable to get Blackmagic Outputdevice" << endl;
		goto error;
	}
	// list video modes for Output

    console() << "Blackmagic Output Modes:" << endl;
	if (pDLOutput->GetDisplayModeIterator(&pDLDisplayModeIterator) == S_OK)
	{
        int i=0;
		while (pDLDisplayModeIterator->Next(&displayMode) == S_OK)
		{
            CFStringRef			modeName;
            displayMode->GetName(&modeName);
            console() << "[" << toString(i) << "] " << convertCfString(modeName) << endl;
            if (i==outputIndex){
                pDLDisplayMode=displayMode;     //found the proper displaymode
            }
            i++;
        }
		pDLDisplayModeIterator->Release();
	}else{
        console() << "Couldn't list Blackmagic Output modes." << endl;
    }
	
	mWidth = pDLDisplayMode->GetWidth();
	mHeight = pDLDisplayMode->GetHeight();	
	
	if (pDLOutput->EnableVideoOutput(pDLDisplayMode->GetDisplayMode(), bmdVideoOutputFlagDefault) != S_OK){
        console() << "Unable to enable Blackmagic Videooutput" << endl;
		goto error;
    
    }
   
	// Flip frame vertical, because OpenGL rendering starts from left bottom corner
	if (pDLOutput->CreateVideoFrame(mWidth, mHeight, mWidth*4, bmdFormat8BitBGRA, bmdFrameFlagFlipVertical, &pDLVideoFrame) != S_OK){
        console() << "Unable to create Blackmagic Video Frame" << endl;
		goto error;
    }
    
	bSuccess = true;

error:
	
	if (!bSuccess)
	{
		if (pDLOutput != NULL)
		{
			pDLOutput->Release();
			pDLOutput = NULL;
		}
		if (pDL != NULL)
		{
			pDL->Release();
			pDL = NULL;
		}
	}
	
	if (pDLIterator != NULL)
	{
		pDLIterator->Release();
		pDLIterator = NULL;
	}
	return bSuccess;
}
/*
bool BlackMagicOutput::InitGUI(IDeckLinkScreenPreviewCallback *previewCallback)
{
	// Set preview
	if (previewCallback != NULL)
	{
		pDLOutput->SetScreenPreviewCallback(previewCallback);
	}
	return true;
}

bool BlackMagicOutput::InitOpenGL()
{
	const GLubyte * strExt;
	GLboolean isFBO;
	
    CGLSetCurrentContext (contextObj);
	
	strExt = glGetString (GL_EXTENSIONS);
	isFBO = gluCheckExtension ((const GLubyte*)"GL_EXT_framebuffer_object", strExt);
	
	if (!isFBO)
	{
		NSRunAlertPanel(@"OpenGL initialization error.", @"OpenGL extention \"GL_EXT_framebuffer_object\" is not supported.", @"OK", nil, nil);
		return false;
	}
    
	return true;
}*/



void BlackMagicOutput::renderToDevice(gl::Fbo * outputFrameBuffer)
{
    
    if (outputFrameBuffer->getWidth()!=mWidth || outputFrameBuffer->getHeight()!=mHeight){
        console() << "Output Format is different from frameBuffer to output." << endl;
        //TODO scale framebuffer instead of returning
        return;
    }
    
	void*	pFrame;
    
	pDLVideoFrame->GetBytes((void**)&pFrame);	
    
    outputFrameBuffer->bindFramebuffer();
    
	glReadPixels(0, 0, mWidth, mHeight, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pFrame);      //read frame buffer to decklickvideoframe
    
    pDLOutput->DisplayVideoFrameSync(pDLVideoFrame);        //display video
    
    outputFrameBuffer->unbindFramebuffer();
	
}

