#pragma region Includes Region

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl2.h"
#include <stdio.h>
#include <GLFW/glfw3.h>

// *** Mine ***
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/if.h>
#include <sys/ioctl.h>
#include <iostream>
#include <typeinfo>
#include <vector>
#include <cstdint> // for uint8_t
#include <atomic>
#include <mutex>
#include <thread>

#define CAN_INTERFACE "can0"
#define CAN_FRAME_SIZE 8

#pragma endregion Includes Region

struct Config
{
    bool civic = true;
    bool mazda = false;
};
Config config;

struct CANBusData
{
    float rpm = 0.0;
    int speed = 0;
    int gear = 0;
    float voltage = 0.0;
    int iat = 0;
    int ect = 0;
    int tps = 0;
    int map = 0;
    float lambdaRatio = 0.0;
    int oilTemp = 0;
    int oilPressure = 0;
};
// Test value display
// struct CANBusData
// {
//     float rpm = 3500.0;
//     int speed = 80;
//     int gear = 4;
//     float voltage = 14.2;
//     int iat = 26;
//     int ect = 91;
//     int tps = 100;
//     int map = 20;
//     float lambdaRatio = 2.0;
//     int oilTemp = 100;
//     int oilPressure = 76;
// };

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Shift byte1 to the left by 8 bits and OR with byte2
uint16_t concatenateBytes(uint8_t byte1, uint8_t byte2)
{
    return (static_cast<uint16_t>(byte1) << 8) | byte2;
}

void readCanData(int s, std::atomic<bool>& running, std::mutex& canDataMutex, CANBusData& canData)
{
    struct can_frame frame;

    while (running) {
        int nbytesread = read(s, &frame, sizeof(struct can_frame));
        if (nbytesread > 0)
        {
            if (config.civic)
            {
                switch (frame.can_id)
                {
                    case 660:
                    case 1632:
                        canData.rpm = static_cast<float>(concatenateBytes(frame.data[0], frame.data[1]));
                        canData.speed = concatenateBytes(frame.data[2], frame.data[3]);
                        canData.gear = frame.data[4];
                        canData.voltage = static_cast<float>(frame.data[5]) / 10;
                        break;
                    case 661:
                    case 1633:
                        canData.iat = concatenateBytes(frame.data[0], frame.data[1]);
                        canData.ect = concatenateBytes(frame.data[2], frame.data[3]);
                        break;
                    case 662:
                    case 1634:
                        canData.tps = concatenateBytes(frame.data[0], frame.data[1]);
                        canData.map = concatenateBytes(frame.data[2], frame.data[3]) / 10;
                        break;
                    case 664:
                    case 1636:
                        canData.lambdaRatio = 32768.0f / static_cast<float>(concatenateBytes(frame.data[0], frame.data[1]));
                        break;
                    case 667:
                    case 1639:
                        canData.oilTemp = concatenateBytes(frame.data[0], frame.data[1]);
                        canData.oilPressure = concatenateBytes(frame.data[2], frame.data[3]);
                        break;
                }
                
                if (canData.tps == 65535)
                  canData.tps = 0;
                
                // Conversions
                // canData.voltage = canData.voltage / 10;
                // canData.map = canData.map / 10;
                // canData.lambdaRatio = 32768 / canData.lambdaRatio;
            }
            
            if (config.mazda) {
                switch (frame.can_id) {
                    case 201:
                    case 513:
                        std::lock_guard<std::mutex> lock(canDataMutex);
                        canData.rpm = ((256 * frame.data[0]) + frame.data[1]) / 4;
                        canData.tps = frame.data[6] / 2;
                        break;
                }
            }
        } else if (nbytesread < 0) {
            perror("can raw socket read");
            running = false;
        }
    }
}

int main(int, char**)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // --------------------------------------- Window setup ---------------------------------------
    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    if (!primaryMonitor)
    {
        glfwTerminate();
        return -1;
    }

    // Get the video mode of the primary monitor
    const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);

    // Set window hints for the new window to reference
    glfwWindowHint(GLFW_RED_BITS, mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

    // Create window with graphics context
    // GLFWwindow* window = glfwCreateWindow(1920, 1080, "Wills Race Dash", nullptr, nullptr);
    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "Wills Race Dash", primaryMonitor, nullptr);
    if (window == nullptr)
    {
        glfwTerminate();
        return 1;
    }

    // --------------------------------------------------------------------------------------------

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL2_Init();

    // Load Fonts
    io.Fonts->AddFontFromFileTTF(".././assets/Calibri.ttf", 100.0f, NULL, io.Fonts->GetGlyphRangesDefault());
    //io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Candara.ttf", 60.0f, NULL, io.Fonts->GetGlyphRangesDefault());

    // Our state
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // ------------------------------ CANBus setup ------------------------------
    int s;
    struct can_frame frame;
    struct sockaddr_can addr;
    struct ifreq ifr;

    // Create socket
    if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
       fprintf(stderr, "Socket creation failed\n");
       // return;
    }

    // Interface setup
    strcpy(ifr.ifr_name, CAN_INTERFACE);
    ioctl(s, SIOCGIFINDEX, &ifr);

    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    // Bind socket to CAN interface
    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
       perror("Binding failed");
       close(s);
       // return;
    }
    // --------------------------------------------------------------------------
    CANBusData canData;

    // Atomic flag for controlling threads
    std::atomic<bool> running(true);

    // Mutex for synchronizing access to canData
    std::mutex canDataMutex;

    // Create a thread for reading CAN data
    std::thread canReaderThread(readCanData, s, std::ref(running), std::ref(canDataMutex), std::ref(canData));
    // --------------------------------------------------------------------------

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        // Start the Dear ImGui frame
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(1920, 1080));
        static float min_row_height = 150.0f;
        static float middle_column_indent = 180.0f;

        // ===============================================================================================================
        ImGui::Begin("Wills Race Dash", 0, ImGuiWindowFlags_NoDecoration);
        ImGui::ProgressBar(canData.rpm / 9000.0f, ImVec2(1905, 100), "");
        
        // ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
        if (ImGui::BeginTable("Data Table", 3,  ImGuiTableFlags_BordersInnerH))
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Coolant:");
            ImGui::TableNextColumn();
            ImGui::Indent(middle_column_indent);
            ImGui::Text("RPM:");
            ImGui::Unindent(middle_column_indent);
            ImGui::TableNextColumn();
            ImGui::Indent(230.0f);
            ImGui::Text("Oil Temp:");
            ImGui::Unindent(230.0f);
            //float textWidth1 = ImGui::CalcTextSize("Oil Temp:").x;
            ImGui::TableNextRow(ImGuiTableRowFlags_None, min_row_height);
            ImGui::TableNextColumn();
            ImGui::Text("%d", canData.ect);
            ImGui::TableNextColumn();
            ImGui::Indent(middle_column_indent);
            ImGui::Text("%.0f", canData.rpm);
            ImGui::Unindent(middle_column_indent);
            ImGui::TableNextColumn();
            ImGui::Indent(470.0f);
            ImGui::Text("%d", canData.oilTemp);
            ImGui::Unindent(470.0f);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("IAT:");
            ImGui::TableNextColumn();
            ImGui::Indent(middle_column_indent);
            ImGui::Text("Speed:");
            ImGui::Unindent(middle_column_indent);
            ImGui::TableNextColumn();
            ImGui::Indent(110.0f);
            ImGui::Text("Oil Pressure:");
            ImGui::Unindent(110.0f);
            ImGui::TableNextRow(ImGuiTableRowFlags_None, min_row_height);
            ImGui::TableNextColumn();
            ImGui::Text("%d", canData.iat);
            ImGui::TableNextColumn();
            ImGui::Indent(middle_column_indent);
            ImGui::Text("%d", canData.speed);
            ImGui::Unindent(middle_column_indent);
            ImGui::TableNextColumn();
            ImGui::Indent(510.0f);
            ImGui::Text("%d", canData.oilPressure);
            ImGui::Unindent(510.0f);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("MAP:");
            ImGui::TableNextColumn();
            ImGui::Indent(middle_column_indent);
            ImGui::Text("Gear:");
            ImGui::Unindent(middle_column_indent);
            ImGui::TableNextColumn();
            ImGui::Indent(450.0f);
            ImGui::Text("TPS:");
            ImGui::Unindent(450.0f);
            ImGui::TableNextRow(ImGuiTableRowFlags_None, min_row_height);
            ImGui::TableNextColumn();
            ImGui::Text("%d", canData.map);
            ImGui::TableNextColumn();
            ImGui::Indent(middle_column_indent);
            ImGui::Text("%d", canData.gear);
            ImGui::Unindent(middle_column_indent);
            ImGui::TableNextColumn();
            ImGui::Indent(460.0f);
            ImGui::Text("%d", canData.tps);
            ImGui::Unindent(460.0f);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Air/Fuel:");
            ImGui::TableNextColumn();
            ImGui::Indent(middle_column_indent);
            ImGui::Text("-");
            ImGui::Unindent(middle_column_indent);
            ImGui::TableNextColumn();
            ImGui::Indent(290.0f);
            ImGui::Text("Voltage:");
            ImGui::Unindent(290.0f);
            ImGui::TableNextRow(ImGuiTableRowFlags_None, min_row_height);
            ImGui::TableNextColumn();
            ImGui::Text("%.1f", canData.lambdaRatio);
            ImGui::TableNextColumn();
            ImGui::Indent(middle_column_indent);
            ImGui::Text("-");
            ImGui::Unindent(middle_column_indent);
            ImGui::TableNextColumn();
            ImGui::Indent(440.0f);
            ImGui::Text("%.1f", canData.voltage);
            ImGui::Unindent(440.0f);

            ImGui::EndTable();
        }

        ImGui::End();
        // ===============================================================================================================

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

        glfwMakeContextCurrent(window);
        glfwSwapBuffers(window);
    }

    canReaderThread.join();

    // Cleanup
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    // Close CANBus socket
    close(s);

    return 0;
}
