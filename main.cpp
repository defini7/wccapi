#include "defGameEngine.hpp"

#define WCCAPI_IMPL
#include "wwccapi.hpp"

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

    wcc::Capturer capturer;
    uint32_t* buffer = nullptr;

protected:
    bool OnUserCreate() override
    {
        int width = GetWindow()->GetScreenWidth();
        int height = GetWindow()->GetScreenHeight();

        if (!capturer.Init(0, width, height, 30))
            return false;

        std::wcout << L"Devices: " << std::endl;

        int i = 1;
        for (const auto& name : wcc::Capturer::EnumerateDevices())
            std::wcout << i++ << L") " << name << '\n';

        std::cout << "\nResolution: " << capturer.GetFrameWidth() << 'x' << capturer.GetFrameHeight() << std::endl;

        buffer = new uint32_t[width * height];
        capturer.SetBuffer(buffer);

        return true;
    }

    bool OnUserUpdate(float) override
    {
        capturer.DoCapture();

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
    if (test.Construct(512, 480, 2, 2))
        test.Run();
}
