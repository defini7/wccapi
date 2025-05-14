#include "defGameEngine.hpp"

#define WWCCAPI_IMPL
#include "wwccapi.hpp"

class Example : public def::GameEngine
{
public:
    Example()
    {
        GetWindow()->SetTitle("Testing webcam");
    }

    wwcc::Capturer capturer;

protected:
    bool OnUserCreate() override
    {
        if (!capturer.Init(0, GetWindow()->GetScreenWidth(), GetWindow()->GetScreenHeight(), 30))
            return false;

        std::wcout << L"Devices: " << std::endl;

        int i = 1;
        for (const auto& name : wwcc::Capturer::EnumerateDevices())
            std::wcout << i++ << L") " << name << '\n';

        std::cout << "\nResolution: " << capturer.GetFrameWidth() << 'x' << capturer.GetFrameHeight() << std::endl;

        return true;
    }

    bool OnUserUpdate(float) override
    {
        uint32_t* pBuffer = capturer.DoCapture();
        if (!pBuffer) return false;

        int width = GetWindow()->GetScreenWidth();
        int height = GetWindow()->GetScreenHeight();

        for (int y = 0; y < height; y++)
            for (int x = 0; x < width; x++)
                Draw(x, y, def::Pixel(pBuffer[y * width + x]));

        return true;
    }

};

int main()
{
    Example test;
    if (test.Construct(512, 480, 2, 2))
        test.Run();
}
