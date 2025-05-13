#include <optional>
#include <string>
#include <list>
#include <iostream>

// https://github.com/defGameEngine/defGameEngine
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

    Capturer c;

protected:
    bool OnUserCreate() override
    {
        if (!c.Init(0, 256, 240))
            return false;

        std::wcout << L"Devices: " << std::endl;

        int i = 1;
        for (const auto& name : Capturer::EnumerateDevices())
            std::wcout << i++ << L") " << name << std::endl;

        return true;
    }

    bool OnUserUpdate(float) override
    {
        uint32_t* pBuffer = c.PerformCapture();
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
    if (test.Construct(256, 240, 4, 4))
        test.Run();
}
