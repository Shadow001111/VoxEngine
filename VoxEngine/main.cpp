#include "WindowManager.h"

int main()
{
    try
    {
        WindowParams params{ 1280, 720, "My OpenGL 4.6 Window", true };
        WindowManager wnd(params);

        // Main loop
        while (!wnd.shouldClose())
        {
            // Rendering
            glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            wnd.swapBuffers();
            wnd.pollEvents();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return -1;
    }
	return 0;
}