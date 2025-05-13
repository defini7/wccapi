// WWCCAPI - Windows WebCam Capturing API

#ifndef WWCCAPI_HPP
#define WWCCAPI_HPP

#include <Windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <Mferror.h>

// TODO: Wrap around with some macros
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mfreadwrite.lib")

enum class VideoFormat
{
    None,
    Rgb32,
    Rgb24,
    Yuy2,
    Nv12
};

class Capturer
{
public:
    Capturer() = default;
    ~Capturer();

    bool Init(unsigned long nDevice, uint32_t nWidth, uint32_t nHeight, uint32_t nFpsNumerator, uint32_t nFpsDenominator = 1);

    static std::list<std::wstring> EnumerateDevices();

    uint32_t* PerformCapture();

    uint32_t GetFrameWidth() const;
    uint32_t GetFrameHeight() const;
    uint32_t GetDeviceCount() const;

    float GetFPS() const;

private:
    bool CreateDevice(const DWORD nDevice);
    bool ConfigureImage(const uint32_t nWidth, const uint32_t nHeight);
    bool ConfigureDecoder();

private:
    IMFSourceReader* m_pReader = nullptr;
    DWORD m_dwStreamIndex = -1;

    IMFMediaSource* m_pDevice = nullptr;
    uint32_t m_nDevices = 0;

    uint8_t* m_pFrame = nullptr;
    uint32_t* m_pOutput = nullptr;

    uint32_t m_nDesiredWidth = 0, m_nDesiredHeight = 0;
    uint32_t m_nFrameWidth = 0, m_nFrameHeight = 0;

    uint32_t m_nFrameStrideYUY2 = 0;
    uint32_t m_nFrameStrideRGB32 = 0;

    VideoFormat m_nVideoFormat = VideoFormat::None;

    uint32_t m_nFpsNumerator = 0;
    uint32_t m_nFpsDenominator = 0;

};

#ifdef WWCCAPI_IMPL
#undef WWCCAPI_IMPL

Capturer::~Capturer()
{
    if (!m_pFrame) delete[] m_pFrame;
    if (!m_pOutput) delete[] m_pOutput;

    if (m_pDevice)
    {
        m_pDevice->Shutdown();
        m_pDevice->Release();
    }

    MFShutdown();
    CoUninitialize();
}

bool Capturer::Init(unsigned long nDeviceID, uint32_t nWidth, uint32_t nHeight, uint32_t nFpsNumerator, uint32_t nFpsDenominator)
{
    m_nFpsNumerator = nFpsNumerator;
    m_nFpsDenominator = nFpsDenominator;

    HRESULT hResult = CoInitialize(nullptr);

    if (FAILED(hResult))
        return false;

    hResult = MFStartup(MF_VERSION);

    if (FAILED(hResult))
        return false;

    if (!CreateDevice(nDeviceID))
        return false;

    if (!ConfigureImage(nWidth, nHeight))
        return false;

    if (!ConfigureDecoder())
        return false;

    return true;
}

bool Capturer::CreateDevice(const DWORD nDeviceID)
{
    IMFAttributes* pConfig = nullptr;
    IMFActivate** ppDevices = nullptr;

    HRESULT hResult = MFCreateAttributes(&pConfig, 1);

    if (FAILED(hResult))
        return false;

    // Request video capture devices
    hResult = pConfig->SetGUID(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

    if (FAILED(hResult))
        return false;

    hResult = MFEnumDeviceSources(pConfig, &ppDevices, &m_nDevices);

    if (FAILED(hResult) || m_nDevices == 0)
        return false;

    hResult = ppDevices[nDeviceID]->ActivateObject(IID_PPV_ARGS(&m_pDevice));

    if (SUCCEEDED(hResult))
    {
        for (DWORD i = 0; i < m_nDevices; i++)
            ppDevices[i]->Release();

        CoTaskMemFree(ppDevices);
    }

    m_dwStreamIndex = nDeviceID;

    return true;
}

std::list<std::wstring> Capturer::EnumerateDevices()
{
    IMFAttributes* pConfig = nullptr;
    IMFActivate** ppDevices = nullptr;

    HRESULT hResult = MFCreateAttributes(&pConfig, 1);

    if (FAILED(hResult))
        return {};

    hResult = pConfig->SetGUID(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
    );

    if (FAILED(hResult))
        return {};

    uint32_t nDevices;
    hResult = MFEnumDeviceSources(pConfig, &ppDevices, &nDevices);

    if (FAILED(hResult) || nDevices == 0)
        return {};

    std::list<std::wstring> listDevices;

    for (DWORD i = 0; i < nDevices; i++)
    {
        WCHAR* sName = nullptr;

        uint32_t nLength;
        HRESULT hResult = ppDevices[i]->GetAllocatedString(
            MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
            &sName, &nLength);

        if (SUCCEEDED(hResult) && sName)
        {
            listDevices.push_back(sName);
            CoTaskMemFree(sName);
        }
    }

    for (DWORD i = 0; i < nDevices; i++)
        ppDevices[i]->Release();

    CoTaskMemFree(ppDevices);
    return listDevices;
}

#define DIE_ON_FAIL(res) if (FAILED(res)) return false;

bool Capturer::ConfigureImage(const uint32_t nWidth, const uint32_t nHeight)
{
    DIE_ON_FAIL(MFCreateSourceReaderFromMediaSource(m_pDevice, NULL, &m_pReader))

    m_nDesiredWidth = nWidth;
    m_nDesiredHeight = nHeight;

    DWORD nIndex = 0;
    uint32_t nBestError = -1;

    IMFMediaType* pNativeType = nullptr;

    // TODO: Fix image size choosing
    while (SUCCEEDED(m_pReader->GetNativeMediaType(m_dwStreamIndex, nIndex, &pNativeType)))
    {
        uint32_t nFrameWidth, nFrameHeight;
        MFGetAttributeSize(pNativeType, MF_MT_FRAME_SIZE, &nFrameWidth, &nFrameHeight);

        // Does the desired size perfectly fits one of the webcam sizes?
        if (nWidth == nFrameWidth && nHeight == nFrameHeight)
        {
            m_nFrameWidth = nFrameWidth;
            m_nFrameHeight = nFrameHeight;
            break;
        }

        // Pick one of the available sizes that's less than the desired size
        // and then choose the closest one

        if (nWidth < nFrameWidth && nHeight < nFrameHeight)
        {
            int nWidthError = abs((int)nWidth - (int)nFrameWidth);
            int nHeightError = abs((int)nHeight - (int)nFrameHeight);

            if (nWidthError < nBestError && nFrameHeight < nBestError)
            {
                m_nFrameWidth = nFrameWidth;
                m_nFrameHeight = nFrameHeight;
                nBestError = max(nWidthError, nHeightError);
            }
        }

        nIndex++;
    }

    if (m_nFrameWidth == 0 || m_nFrameHeight == 0)
        return false;

    m_nFrameStrideYUY2 = m_nFrameWidth * 2;
    m_nFrameStrideRGB32 = m_nFrameWidth * 4;

    m_pFrame = new uint8_t[m_nFrameStrideRGB32 * m_nFrameHeight];
    m_pOutput = new uint32_t[m_nDesiredWidth * m_nDesiredHeight];

    return true;
}

#define DIE_ON_FAIL(res) if (!(res)) { bResult = false; goto end; }

bool Capturer::ConfigureDecoder()
{
    IMFMediaType* pNativeType = nullptr;

    if (FAILED(m_pReader->GetCurrentMediaType(m_dwStreamIndex, &pNativeType)))
        return false;

    bool bResult = true;
    IMFMediaType* pType = nullptr;
    GUID guidMajorType;

    // Changing the default media type

    DIE_ON_FAIL(SUCCEEDED(pNativeType->GetGUID(MF_MT_MAJOR_TYPE, &guidMajorType)))
    DIE_ON_FAIL(SUCCEEDED(MFCreateMediaType(&pType)))
    DIE_ON_FAIL(SUCCEEDED(pType->SetGUID(MF_MT_MAJOR_TYPE, guidMajorType)))

    DIE_ON_FAIL(guidMajorType == MFMediaType_Video)

    /* MFVideoFormat_RGB32
        MFVideoFormat_RGB24
        MFVideoFormat_YUY2
        MFVideoFormat_NV12 */

    // TODO: Add an ability to change formats
    DIE_ON_FAIL(SUCCEEDED(pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_YUY2)))
    DIE_ON_FAIL(SUCCEEDED(MFSetAttributeRatio(pType, MF_MT_FRAME_RATE, m_nFpsNumerator, m_nFpsDenominator)))
    DIE_ON_FAIL(SUCCEEDED(m_pReader->SetCurrentMediaType(m_dwStreamIndex, nullptr, pType)))

end:
    pNativeType->Release();
    pType->Release();

    return bResult;
}

#undef DIE_ON_FAIL

#define DIE_IF(res) if (res) goto end;

uint32_t* Capturer::PerformCapture()
{
    IMFSample* pSample = nullptr;

    while (true)
    {
        DWORD nFlags;

        while (1)
        {
            HRESULT hResult = m_pReader->ReadSample(
                m_dwStreamIndex, 0, nullptr,
                &nFlags, nullptr, &pSample
            );

            DIE_IF(FAILED(hResult))

            if ((nFlags & MF_SOURCE_READERF_STREAMTICK) == 0 && pSample)
                break;
        }

        DIE_IF(nFlags & MF_SOURCE_READERF_ENDOFSTREAM)

        if (nFlags & MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED)
        {
            // The format has changed
            DIE_IF(!ConfigureDecoder())
        }

        IMFMediaBuffer* pBuffer = nullptr;
        DIE_IF(!pSample || FAILED(pSample->ConvertToContiguousBuffer(&pBuffer)))

        uint8_t* pData = nullptr;

        if (FAILED(pBuffer->Lock(&pData, nullptr, nullptr)))
        {
            pBuffer->Release();
            goto end;
        }

        // TODO: Add different formats
        // now we convert: YUY2 -> RGB32

        auto clamp = [](int value) -> uint8_t
            {
                if (value < 0) return 0;
                if (value > 255) return 255;
                return value;
            };

        auto yuv_to_rgb = [&clamp](int y, int u, int v, uint8_t* pBuffer)
            {
                int c = y - 16;
                int d = u - 128;
                int e = v - 128;

                // |R|   |1.164 0.000  1.596  |   |y-16 |
                // |G| = |1.164 -0.391 -0.813 | * |u-128|
                // |B|   |1.164 2.018  0.000  |   |v-128|

                // RGBA
                pBuffer[0] = clamp((298 * c + 409 * e + 128) >> 8);
                pBuffer[1] = clamp((298 * c - 100 * d - 208 * e + 128) >> 8);
                pBuffer[2] = clamp((298 * c + 516 * d + 128) >> 8);
                pBuffer[3] = 255;
            };

        for (uint32_t y = 0; y < m_nFrameHeight; y++)
        {
            uint8_t* pSrcRow = pData + y * m_nFrameStrideYUY2;
            uint8_t* pDstRow = m_pFrame + y * m_nFrameStrideRGB32;

            for (uint32_t x = 0; x < m_nFrameWidth; x += 2)
            {
                uint8_t y0 = pSrcRow[0];
                uint8_t u = pSrcRow[1];
                uint8_t y1 = pSrcRow[2];
                uint8_t v = pSrcRow[3];
                pSrcRow += 4;

                yuv_to_rgb(y0, u, v, pDstRow + x * 4);
                yuv_to_rgb(y1, u, v, pDstRow + x * 4 + 4);
            }
        }

        uint32_t* pDst = m_pOutput;
        uint32_t* pSrc = reinterpret_cast<uint32_t*>(m_pFrame);

        for (uint32_t y = 0; y < m_nDesiredHeight; y++)
            for (uint32_t x = 0; x < m_nDesiredWidth; x++, pDst++)
            {
                size_t i = y * m_nFrameHeight / m_nDesiredHeight * m_nFrameWidth + x * m_nFrameWidth / m_nDesiredWidth;
                *pDst = pSrc[i];
            }

        pBuffer->Release();
        break;
    }

end:
    if (pSample)
        pSample->Release();

    return m_pOutput;
}

uint32_t Capturer::GetFrameWidth() const { return m_nFrameWidth; }
uint32_t Capturer::GetFrameHeight() const { return m_nFrameHeight; }
uint32_t Capturer::GetDeviceCount() const { return m_nDevices; }

float Capturer::GetFPS() const
{
    return (float)m_nFpsNumerator / (float)m_nFpsDenominator;
}

#endif

#endif
