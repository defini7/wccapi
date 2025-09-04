#import <AVFoundation/AVFoundation.h>
#include <Foundation/Foundation.h>

typedef unsigned int u32;

@interface Capturer_MacOS : NSObject
{
    AVCaptureSession* mSession;
	AVCaptureDevice* mDevice;
	AVCaptureOutput* mDataOut;
	AVCaptureInput* mDataIn;

@public
    u32 mDesiredWidth;
    u32 mDesiredHeight;
    u32 mActualWidth;
    u32 mActualHeight;
    u32 mID;
    u32 mFpsNumerator;
    u32 mFpsDenominator;

}

- (instancetype)init;
- (BOOL)Init: (u32)deviceID width:(u32)w height:(u32)h fpsNumerator:(u32)fpsNum fpsDenominator:(u32)fpsDen;
- (BOOL)CreateDevice: (u32)deviceID;
- (NSMutableArray*)EnumerateDevices;
- (BOOL)ConfigureImage: (u32)w height:(u32)h;
- (void)DoCapture;

- (NSArray*)_GetDevices;

@end

@implementation Capturer_MacOS

- (instancetype)init
{
    self = [super init];

    if (self)
    {
        mDesiredWidth = 0;
        mDesiredHeight = 0;
        mActualWidth = 0;
        mActualHeight = 0;
        mID = -1;
        mFpsNumerator = 0;
        mFpsDenominator = 0;
    }

    return self;
}

- (BOOL)Init: (u32)deviceID width:(u32)w height:(u32)h fpsNumerator:(u32)fpsNum fpsDenominator:(u32)fpsDen
{
    mFpsNumerator = fpsNum;
    mFpsDenominator = fpsDen;

    if (![self CreateDevice:deviceID])
        return NO;

    if (![self ConfigureImage:w height:h])
}

- (BOOL)CreateDevice: (u32)deviceID
{
    NSArray* devices = [self _GetDevices];

    // There are no devices or an ID is invalid
    if (deviceID >= [devices count])
        return NO;

    mID = deviceID;
    mDevice = [devices objectAtIndex:mID];

    return YES;
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

- (BOOL)ConfigureImage: (u32)w height:(u32)h
{
    NSError* error = nil;
	[mDevice lockForConfiguration:&error];

	if (error)
    {
        // Can't lock for configuration
        return NO;
    }

    mDesiredWidth = w;
    mDesiredHeight = h;

    AVCaptureDeviceFormat* bestFormat = nil;
    CMVideoDimensions bestSize;

    u32 bestError = -1;

    // TODO: If it's guaranteed that all resolutions are sorted then
    // we can peek the first one that's greater than the desired one
    // and exit the loop instantly

    for (AVCaptureDeviceFormat* format in [mDevice formats])
    {
        CMVideoDimensions size = CMVideoFormatDescriptionGetDimensions(format.formatDescription);

        // Does the desired size perfectly fits one of the webcam sizes?
        if (size.width == mDesiredWidth && size.height == mDesiredHeight)
        {
            bestFormat = format;
            bestSize = size;
            break;
        }

        if (size.width <= mDesiredWidth && size.height <= mDesiredHeight)
        {
            int widthError = (int)mDesiredWidth - (int)size.width;
            int heightError = (int)mDesiredHeight - (int)size.height;

            if (widthError < bestError && heightError < bestError)
            {
                mActualWidth = size.width;
                mActualHeight = size.height;
                bestError = MAX(widthError, heightError);
            }
        }
    }
}

- (void)DoCapture
{

}

- (NSArray*)_GetDevices
{
    NSArray* devices;

    // devicesWithMediaType from AVCaptureDevice was deprecated in macOS 10.15 (Catalina)
    if(@available(macOS 10.15, *))
    {
        NSArray* deviceTypes = @[AVCaptureDeviceTypeBuiltInWideAngleCamera];

		AVCaptureDeviceDiscoverySession* ds =
            [AVCaptureDeviceDiscoverySession
                discoverySessionWithDeviceTypes:deviceTypes
                mediaType:AVMediaTypeVideo
                position:AVCaptureDevicePositionUnspecified];
		
        devices	= [ds devices];
    }
    else
        devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];

    return devices;
}

@end
