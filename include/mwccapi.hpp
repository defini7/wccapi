/* GENERAL INFO

    mwccapi.hpp
    
    +----------------------------------+
    |             WCCAPI               |
    |       WebCam Capturing API       |
    +----------------------------------+
    
    
    Distributed under GPL3 license
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

                    GNU GENERAL PUBLIC LICENSE
                      Version 3, 29 June 2007
     
    Copyright (C) 2007 Free Software Foundation, Inc. <https://fsf.org/>
    Everyone is permitted to copy and distribute verbatim copies
    of this license document, but changing it is not allowed.
    

    Author
    ~~~~~~

    Alex, aka defini7, Copyright (C) 2025
    
*/

/* VERSION HISTORY

    0.01: Added support for RGB32, RGB24, YUY2 formats on Windows platform
    0.02: Added support for macOS
*/

#ifndef MWCCAPI_H
#define MWCCAPI_H

#ifndef __APPLE__
#error You can't use macOS version of WCCAPI
#endif

#ifndef __OBJC__
#error You need to compile it as an Objective-C++ code
#endif

#include <cstdint>
#include <vector>
#include <string>
#include <optional>

namespace mwcc
{
    struct CaptureParams
    {
        uint32_t desiredWidth = 0;
        uint32_t desiredHeight = 0;

        uint32_t actualWidth = 0;
        uint32_t actualHeight = 0;

        uint32_t id = -1;

        float fps = 0.0f;

        bool isFrameReady = false;
        bool wantCapture = false;

        uint8_t* source = nullptr;
        uint32_t* output = nullptr;
    };
}

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>
#import <Accelerate/Accelerate.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>

@interface _Capturer_MacOS : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
{
    AVCaptureSession* mSession;
    AVCaptureDevice* mDevice;
    AVCaptureVideoDataOutput* mDataOut;
    AVCaptureDeviceInput* mDataIn;

@public
    mwcc::CaptureParams mCapParams;

}

- (void)dealloc;

- (bool)Init: (uint32_t)deviceID width:(uint32_t)w height:(uint32_t)h framerate:(float)fps;
- (bool)CreateDevice: (uint32_t)deviceID;
- (NSMutableArray*)EnumerateDevices;
- (bool)ConfigureImage: (uint32_t)w height:(uint32_t)h;
- (bool)DoCapture;

- (void)Start;
- (void)Stop;

- (NSArray*)_GetDevices;
- (void)_SetSourceFromBGRA: (uint8_t*)input;

@end

namespace mwcc
{
    // Opens a physical device using its ID, configures it by picking
    // very close resolution to the desired one, picks RGBA as an image format,
    // and allocates memory in a buffer for the capture.
    // TODO: framerate support
    std::optional<std::reference_wrapper<CaptureParams>> Init(uint32_t deviceID, uint32_t frameWidth, uint32_t frameHeight, float framerate);

    // Releases all devices and frees memory.
    void Stop();

    // Finds all webcams on your machine and returns their names.
    std::vector<std::wstring> EnumerateDevices();

    // Returns true if the frame is ready.
    bool DoCapture();

    // buffer must be at least sizeof(uint32_t) * m_nDesiredWidth * m_nDesiredHeight in size
    void SetBuffer(uint32_t* buffer);
}

#ifdef MWCCAPI_IMPL
#undef MWCCAPI_IMPL

@implementation _Capturer_MacOS

- (void)dealloc
{
    [self Stop];

    if (mSession)
		[mSession release];

    if (mDataOut)
    {
        if (mDataOut.sampleBufferDelegate)
            [mDataOut setSampleBufferDelegate:nil queue:nil];

        [mDataOut release];
    }

    if (mDataIn)
        [mDataIn release];

    if (mDevice)
        [mDevice release];

    if (mCapParams.source)
        free(mCapParams.source);

    [super dealloc];
}

- (bool)Init: (uint32_t)deviceID width:(uint32_t)w height:(uint32_t)h framerate:(float)fps
{
    if (w <= 0 || h <= 0 || fps <= 0.0f)
        return false;

    mCapParams.fps = fps;

    if (![self CreateDevice:deviceID])
        return false;

    if (![self ConfigureImage:w height:h])
        return false;

    return true;
}

- (bool)CreateDevice: (uint32_t)deviceID
{
    NSArray* devices = [self _GetDevices];

    // There are no devices or an ID is invalid
    if (deviceID >= [devices count])
        return false;

    mCapParams.id = deviceID;
    mDevice = [devices objectAtIndex:deviceID];

    return true;
}

- (NSArray*)EnumerateDevices
{
    NSArray* devices = [self _GetDevices];

    if ([devices count] == 0)
        return nil;

    NSMutableArray<NSString*>* names = [NSMutableArray new];

    for (AVCaptureDevice* device in devices)
        [names addObject:[device localizedName]];

    return names;
}

- (bool)ConfigureImage: (uint32_t)w height:(uint32_t)h
{
    NSError* error = nil;
	[mDevice lockForConfiguration:&error];

	if (error)
    {
        // Can't lock for configuration
        return false;
    }

    mCapParams.desiredWidth = w;
    mCapParams.desiredHeight = h;

    AVCaptureDeviceFormat* bestFormat = nil;
    CMVideoDimensions bestSize;

    uint32_t bestError = -1;

    // TODO: If it's guaranteed that all resolutions are sorted then
    // we can peek the first one that's greater than the desired one
    // and exit the loop instantly

    for (AVCaptureDeviceFormat* format in [mDevice formats])
    {
        CMVideoDimensions size = CMVideoFormatDescriptionGetDimensions(format.formatDescription);

        // Does the desired size perfectly fits one of the webcam sizes?
        if (size.width == mCapParams.desiredWidth && size.height == mCapParams.desiredHeight)
        {
            bestFormat = format;
            bestSize = size;
            break;
        }

        if (size.width > mCapParams.desiredWidth && size.height > mCapParams.desiredHeight)
        {
            uint32_t widthError = size.width - mCapParams.desiredWidth;
            uint32_t heightError = size.height - mCapParams.desiredHeight;

            if (widthError < bestError && heightError < bestError)
            {
                bestSize = size;
                bestError = std::max(widthError, heightError);
                bestFormat = format;
            }
        }
    }

    if (bestFormat)
    {
		[mDevice setActiveFormat:bestFormat];
        mCapParams.actualWidth = bestSize.width;
        mCapParams.actualHeight = bestSize.height;
    }
    else
        return false;

    // For whatever reason the only available FPSs are 15 and 30

    // Searching for the closest available FPS to the requested one
    bool isFpsConfigured = false;

    for(AVFrameRateRange* range in mDevice.activeFormat.videoSupportedFrameRateRanges)
    {
        if (floor([range minFrameRate]) <= mCapParams.fps && mCapParams.fps <= ceil([range maxFrameRate]))
        {
            mDevice.activeVideoMinFrameDuration = range.minFrameDuration;
            mDevice.activeVideoMaxFrameDuration = range.maxFrameDuration;
            isFpsConfigured = true;
            break;
        }
    }

    if (!isFpsConfigured)
        return false;

    [mDevice unlockForConfiguration];

    mDataIn = [AVCaptureDeviceInput deviceInputWithDevice:mDevice error:nil];
	mDataOut = [AVCaptureVideoDataOutput new];
	mDataOut.alwaysDiscardsLateVideoFrames = YES;

    // Running capturing on a different thread
    dispatch_queue_t queue = dispatch_queue_create("VideoStream", nullptr);
	dispatch_set_target_queue(queue, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0));

	[mDataOut setSampleBufferDelegate:self queue:queue];
	dispatch_release(queue);

    // RGBA is not supported (at least on my machine) so let's use BGRA and convert it to RGBA manually
    NSDictionary* settings = [
        NSDictionary dictionaryWithObjectsAndKeys:
        [NSNumber numberWithUnsignedInt:kCVPixelFormatType_32BGRA], kCVPixelBufferPixelFormatTypeKey,
        [NSNumber numberWithInt:mCapParams.actualWidth], kCVPixelBufferWidthKey,
        [NSNumber numberWithInt:mCapParams.actualHeight], kCVPixelBufferHeightKey, nil];

	[mDataOut setVideoSettings:settings];

    mSession = [AVCaptureSession new];
    [mSession beginConfiguration];
	[mSession addInput:mDataIn];
	[mSession addOutput:mDataOut];

    [mSession commitConfiguration];

    mCapParams.source = (uint8_t*)malloc(mCapParams.actualWidth * mCapParams.actualHeight * 4);

    return true;
}

- (bool)DoCapture
{
    // Since an image capturing is based on the callbacks
    // we need to notify a callback function that we want to
    // capture an image if it is ready.

    mCapParams.wantCapture = true;
    bool isFrameReady = mCapParams.isFrameReady;

    if (isFrameReady)
        mCapParams.isFrameReady = false;

    return isFrameReady;
}

- (void)Start
{
    [mSession startRunning];
	[mDataIn.device lockForConfiguration:nil];

	if([mDataIn.device isFocusModeSupported:AVCaptureFocusModeAutoFocus])
		[mDataIn.device setFocusMode:AVCaptureFocusModeAutoFocus];
}

- (void)Stop
{
    if (mSession)
    {
        if (mDataOut)
        {
            if (mDataOut.sampleBufferDelegate)
                [mDataOut setSampleBufferDelegate:nil queue:nil];
        }

        for (AVCaptureInput* i in mSession.inputs)
            [mSession removeInput:i];

        for (AVCaptureOutput* o in mSession.outputs)
            [mSession removeOutput:o];

        [mSession stopRunning];
    }
}

- (void)captureOutput:(AVCaptureOutput*)captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection*)connection
{
    if (!mCapParams.wantCapture)
        return;

    @autoreleasepool
    {
        CVImageBufferRef buffer = CMSampleBufferGetImageBuffer(sampleBuffer);
		CVPixelBufferLockBaseAddress(buffer, 0);
        
        // Convert from BGRA to RGBA.
        [self _SetSourceFromBGRA: (uint8_t*)CVPixelBufferGetBaseAddress(buffer)];

        // Downscale a frame
        uint32_t* dst = mCapParams.output;
        uint32_t* src = reinterpret_cast<uint32_t*>(mCapParams.source);

        for (uint32_t y = 0; y < mCapParams.desiredHeight; y++)
            for (uint32_t x = 0; x < mCapParams.desiredWidth; x++, dst++)
            {
                uint32_t i = y * mCapParams.actualHeight / mCapParams.desiredHeight * mCapParams.actualWidth + x * mCapParams.actualWidth / mCapParams.desiredWidth;
                *dst = src[i];
            }

        mCapParams.isFrameReady = true;

		CVPixelBufferUnlockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
    }
}

- (NSArray*)_GetDevices
{
    NSArray* devices;

    // devicesWithMediaType from AVCaptureDevice was deprecated in macOS 10.15 (Catalina)
    #if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500

    NSArray* deviceTypes = @[AVCaptureDeviceTypeBuiltInWideAngleCamera];

    AVCaptureDeviceDiscoverySession* ds =
        [AVCaptureDeviceDiscoverySession
            discoverySessionWithDeviceTypes:deviceTypes
            mediaType:AVMediaTypeVideo
            position:AVCaptureDevicePositionUnspecified];
    
    devices	= [ds devices];

    #else

    devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];

    #endif

    return devices;
}

- (void)_SetSourceFromBGRA: (uint8_t*)input
{
    uint32_t size = mCapParams.actualWidth * mCapParams.actualHeight * 4;
    memcpy(mCapParams.source, input, size);

    for (uint32_t i = 0; i < size; i += 4)
        std::swap(mCapParams.source[i], mCapParams.source[i + 2]);
}

@end

namespace mwcc
{

static _Capturer_MacOS* gCapturer;

std::optional<std::reference_wrapper<CaptureParams>> Init(uint32_t deviceID, uint32_t frameWidth, uint32_t frameHeight, float framerate)
{
    gCapturer = [_Capturer_MacOS new];

    if (![gCapturer Init:deviceID width:frameWidth height:frameHeight framerate:framerate])
        return std::nullopt;

    [gCapturer Start];
        
    return gCapturer->mCapParams;
}

void Stop()
{
    [gCapturer Stop];
}

std::vector<std::wstring> EnumerateDevices()
{
    NSMutableArray* devices = [gCapturer EnumerateDevices];
    std::vector<std::wstring> names([devices count]);

    size_t i = 0;
    for (NSString* name in devices)
    {
        const char* n = [name UTF8String];
        names[i] = std::wstring(n, n + [name length]);
        i++;
    }

    return names;
}

bool DoCapture()
{
    return [gCapturer DoCapture];
}

void SetBuffer(uint32_t* buffer)
{
    gCapturer->mCapParams.output = buffer;
}

}

#endif

#endif
