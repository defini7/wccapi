#include "defGameEngine.hpp"

#ifdef __APPLE__

#define MWCCAPI_IMPL
#include "../Include/mwccapi.hpp"

#elif defined(_WIN32)

#define WWCCAPI_IMPL
#include "../Include/wwccapi.hpp"

#endif

class Example : public def::GameEngine
{
public:
    Example()
    {
        GetWindow()->SetTitle("Testing webcam");
    }

    ~Example()
    {
        delete[] buffer;
    }

    uint32_t* buffer = nullptr;

    #ifdef _WIN32
    wwcc::Capturer capturer;
    #endif

    #ifdef __APPLE__
    mwcc::CaptureParams capParams;
    #endif

protected:
    bool OnUserCreate() override
    {
        int width = GetWindow()->GetScreenWidth();
        int height = GetWindow()->GetScreenHeight();

        buffer = new uint32_t[width * height];

        #ifdef __APPLE__

        if (auto cp = mwcc::Init(0, width, height, 30))
            capParams = *cp;
        else
            return false;

        std::list<std::wstring> devices = mwcc::EnumerateDevices();
        mwcc::SetBuffer(buffer);

        #elif defined(_WIN32)

        if (!capturer.Init(0, width, height, 30))
            return false;

        std::list<std::wstring> devices = wwcc::Capturer::EnumerateDevices();
        capturer.SetBuffer(buffer);

        #endif

        std::wcout << L"Devices: " << std::endl;

        int i = 1;        
        for (const auto& name : devices)
            std::wcout << i++ << L") " << name << '\n';

        #ifdef _WIN32
        std::cout << "\nResolution: " << capturer.GetFrameWidth() << 'x' << capturer.GetFrameHeight() << std::endl;
        #endif

        #ifdef __APPLE__
        std::cout << "\nResolution: " << capParams.actualWidth << 'x' << capParams.actualHeight << std::endl;
        #endif

        return true;
    }

    bool OnUserUpdate(float) override
    {
        #ifdef _WIN32
        capturer.DoCapture();
        #endif

        #ifdef __APPLE__
        mwcc::DoCapture();
        #endif

        int width = GetWindow()->GetScreenWidth();
        int height = GetWindow()->GetScreenHeight();

        for (int y = 0; y < height; y++)
            for (int x = 0; x < width; x++)
                Draw(x, y, def::Pixel(buffer[y * width + x]));

        return true;
    }

};

int main()
{
    Example test;

    if (test.Construct(256, 240, 4, 4))
        test.Run();
}
