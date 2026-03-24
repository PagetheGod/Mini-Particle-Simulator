#pragma once


/*
* This class will handle the GUI application, including:
* 1. Initialization and init error handling. It will set up both ImGUI and SDL3, and Vulkan backend(optional)
* 2. Run the main loop - poll events, draw the immediate mode UI for ImGUI, and issue draw calls
* 3. Initialize thread pool, fill the particle object pool
*/
class Application {
public:
    //Constructors and destructors
    Application();
    Application(const Application&) = delete; //Makes no sense to copy/move the application instance in our case
    Application(Application&&) = delete;
    Application& operator=(const Application&) = delete;
    ~Application();

    //Init and frame function
    bool Initialize();
    void Frame();

//Separate access modifiers to help organize functions and variables
public:


private:

private:
};


