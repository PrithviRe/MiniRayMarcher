#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "Shader.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }

    Vec3 normalize() const {
        float len = std::sqrt(x * x + y * y + z * z);
        if (len > 0.00001f)
            return Vec3(x / len, y / len, z / len);
        return *this;
    }

    static Vec3 cross(const Vec3& a, const Vec3& b) {
        return Vec3(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        );
    }
};

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Mini Ray Marcher", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, int w, int h) {
        glViewport(0, 0, w, h);
    });

    Shader shader(SHADER_DIR "/raymarch.vs", SHADER_DIR "/raymarch.fs");

    // Fullscreen quad
    float quadVertices[] = {
        -1.0f,  1.0f,
        -1.0f, -1.0f,
         1.0f, -1.0f,

        -1.0f,  1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f
    };

    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    // Camera state
    Vec3 camPos(0.0f, 2.5f, 6.0f);
    float yaw = -90.0f;
    float pitch = -15.0f;

    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
    bool firstMouse = true;

    int activeScene = 0;
    bool shadowsEnabled = true;
    bool aoEnabled = true;
    bool animateLight = true;

    double lastTime = glfwGetTime();

    std::cout << "========================================\n"
              << "          Mini Ray Marcher\n"
              << "========================================\n"
              << " Left/Right Mouse Drag : Look / Rotate camera\n"
              << " W / A / S / D         : Move camera forward/back/left/right\n"
              << " Space / Left Shift    : Move camera up / down\n"
              << " Key '1'               : Scene 1: Morphing Blobs & CSG Shapes\n"
              << " Key '2'               : Scene 2: Geometric CSG & Pillars\n"
              << " Key '3'               : Scene 3: Infinite Hall of Columns\n"
              << " Key 'L'               : Toggle Light Orbit\n"
              << " Key 'F'               : Toggle Soft Shadows\n"
              << " Key 'O'               : Toggle Ambient Occlusion\n"
              << " ESC                   : Exit\n"
              << "========================================\n";

    while (!glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;
        deltaTime = std::min(deltaTime, 0.05f);

        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // Scene Switching
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) activeScene = 0;
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) activeScene = 1;
        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) activeScene = 2;

        // Toggle options
        static bool lPressed = false;
        if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) {
            if (!lPressed) { animateLight = !animateLight; lPressed = true; }
        } else { lPressed = false; }

        static bool fPressed = false;
        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
            if (!fPressed) { shadowsEnabled = !shadowsEnabled; fPressed = true; }
        } else { fPressed = false; }

        static bool oPressed = false;
        if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS) {
            if (!oPressed) { aoEnabled = !aoEnabled; oPressed = true; }
        } else { oPressed = false; }

        // Mouse look
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        if (firstMouse) {
            lastMouseX = mouseX;
            lastMouseY = mouseY;
            firstMouse = false;
        }

        double xOffset = mouseX - lastMouseX;
        double yOffset = lastMouseY - mouseY; // Inverted
        lastMouseX = mouseX;
        lastMouseY = mouseY;

        bool mouseHeld = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS ||
                          glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);

        if (mouseHeld) {
            constexpr float sensitivity = 0.2f;
            yaw += static_cast<float>(xOffset) * sensitivity;
            pitch += static_cast<float>(yOffset) * sensitivity;
            pitch = std::clamp(pitch, -89.0f, 89.0f);
        }

        // Camera direction vectors
        float radYaw = yaw * (3.14159265f / 180.0f);
        float radPitch = pitch * (3.14159265f / 180.0f);

        Vec3 front;
        front.x = std::cos(radYaw) * std::cos(radPitch);
        front.y = std::sin(radPitch);
        front.z = std::sin(radYaw) * std::cos(radPitch);
        front = front.normalize();

        Vec3 right = Vec3::cross(front, Vec3(0.0f, 1.0f, 0.0f)).normalize();
        Vec3 up(0.0f, 1.0f, 0.0f);

        // Keyboard movement
        float speed = 4.5f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) speed *= 1.8f;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camPos = camPos + front * speed;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camPos = camPos - front * speed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camPos = camPos + right * speed;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camPos = camPos - right * speed;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) camPos = camPos + up * speed;
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) camPos = camPos - up * speed;

        Vec3 camTarget = camPos + front;

        // Light position
        Vec3 lightPos(4.0f, 7.0f, 3.0f);
        if (animateLight) {
            float lightAngle = static_cast<float>(currentTime) * 0.8f;
            lightPos.x = std::cos(lightAngle) * 6.0f;
            lightPos.z = std::sin(lightAngle) * 6.0f;
            lightPos.y = 5.5f + 1.5f * std::sin(lightAngle * 1.5f);
        }

        int winWidth, winHeight;
        glfwGetFramebufferSize(window, &winWidth, &winHeight);

        glClear(GL_COLOR_BUFFER_BIT);

        shader.use();
        shader.setVec2("u_Resolution", static_cast<float>(winWidth), static_cast<float>(winHeight));
        shader.setFloat("u_Time", static_cast<float>(currentTime));
        shader.setVec3("u_CamPos", camPos.x, camPos.y, camPos.z);
        shader.setVec3("u_CamTarget", camTarget.x, camTarget.y, camTarget.z);
        shader.setVec3("u_LightPos", lightPos.x, lightPos.y, lightPos.z);
        shader.setInt("u_SceneIndex", activeScene);
        shader.setInt("u_ShadowsEnabled", shadowsEnabled ? 1 : 0);
        shader.setInt("u_AOEnabled", aoEnabled ? 1 : 0);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glfwSwapBuffers(window);
    }

    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &VAO);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

