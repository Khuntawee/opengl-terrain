#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader_m.h>
#include <learnopengl/camera.h>

#include <iostream>
#include <vector>
#include <cmath>

#define STB_PERLIN_IMPLEMENTATION
#include "stb_perlin.h"

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
unsigned int loadTexture(const char *path);

// terrain helpers
void generateTerrain(std::vector<float>& vertices, std::vector<unsigned int>& indices, int N, float scale, float offsetX, float offsetZ, float amplitude, float freq);
float sampleHeight(float x, float z, float offsetX, float offsetZ, float amplitude, float freq);

// settings
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// camera
Camera camera(glm::vec3(0.0f, 50.0f, 100.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// terrain state
const int GRID_N = 256;      
float terrainScale = 0.5f;              // distance between grid points
float terrainAmplitude = 30.0f;        // max height
float terrainFreq = 0.02f;             // base frequency
float terrainOffsetX = 0.0f;           // moves when pressing WASD/arrows
float terrainOffsetZ = 0.0f;

// dynamic buffers
unsigned int terrainVAO = 0, terrainVBO = 0, terrainEBO = 0;
size_t terrainIndexCount = 0;

int main()
{
    // glfw: initialize and configure
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "3D Kinetic Terrain", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    // shaders
    Shader lightingShader("6.multiple_lights.vs", "6.multiple_lights.fs");

    // create terrain buffers
    glGenVertexArrays(1, &terrainVAO);
    glGenBuffers(1, &terrainVBO);
    glGenBuffers(1, &terrainEBO);

    // initial terrain data
    std::vector<float> terrainVertices;
    std::vector<unsigned int> terrainIndices;
    generateTerrain(terrainVertices, terrainIndices, GRID_N, terrainScale, terrainOffsetX, terrainOffsetZ, terrainAmplitude, terrainFreq);
    terrainIndexCount = terrainIndices.size();

    glBindVertexArray(terrainVAO);
    glBindBuffer(GL_ARRAY_BUFFER, terrainVBO);
    glBufferData(GL_ARRAY_BUFFER, terrainVertices.size() * sizeof(float), terrainVertices.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, terrainEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, terrainIndices.size() * sizeof(unsigned int), terrainIndices.data(), GL_STATIC_DRAW);

    // layout
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // setup point lights positions
    glm::vec3 pointLightPositions[] = {
        glm::vec3( 50.0f,  60.0f,  50.0f),
        glm::vec3( 100.0f,  80.0f, -40.0f),
        glm::vec3(-60.0f,  70.0f, -120.0f),
        glm::vec3( 0.0f,   65.0f, -50.0f)
    };

    // shader configuration
    lightingShader.use();
    lightingShader.setFloat("material.shininess", 32.0f);

    // render loop
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        // update terrain if offsets changed
        static float lastOffsetX = terrainOffsetX;
        static float lastOffsetZ = terrainOffsetZ;
        if (std::abs(lastOffsetX - terrainOffsetX) > 1e-6f || std::abs(lastOffsetZ - terrainOffsetZ) > 1e-6f)
        {
            generateTerrain(terrainVertices, terrainIndices, GRID_N, terrainScale, terrainOffsetX, terrainOffsetZ, terrainAmplitude, terrainFreq);
            glBindBuffer(GL_ARRAY_BUFFER, terrainVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, terrainVertices.size() * sizeof(float), terrainVertices.data());
            lastOffsetX = terrainOffsetX;
            lastOffsetZ = terrainOffsetZ;
        }

        // render
        glClearColor(0.2f, 0.25f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // use lighting shader
        lightingShader.use();
        lightingShader.setVec3("viewPos", camera.Position);

        // directional light
        lightingShader.setVec3("dirLight.direction", -0.2f, -1.0f, -0.3f);
        lightingShader.setVec3("dirLight.ambient", 0.05f, 0.05f, 0.05f);
        lightingShader.setVec3("dirLight.diffuse", 0.4f, 0.4f, 0.4f);
        lightingShader.setVec3("dirLight.specular", 0.5f, 0.5f, 0.5f);

        // point lights
        for (int i = 0; i < 4; ++i) {
            std::string idx = std::to_string(i);
            lightingShader.setVec3(("pointLights[" + idx + "].position").c_str(), pointLightPositions[i]);
            lightingShader.setVec3(("pointLights[" + idx + "].ambient").c_str(), 0.05f, 0.05f, 0.05f);
            lightingShader.setVec3(("pointLights[" + idx + "].diffuse").c_str(), 0.8f, 0.8f, 0.8f);
            lightingShader.setVec3(("pointLights[" + idx + "].specular").c_str(), 1.0f, 1.0f, 1.0f);
            lightingShader.setFloat(("pointLights[" + idx + "].constant").c_str(), 1.0f);
            lightingShader.setFloat(("pointLights[" + idx + "].linear").c_str(), 0.002f);
            lightingShader.setFloat(("pointLights[" + idx + "].quadratic").c_str(), 0.0002f);
        }

        // view/projection
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 200.0f);
        glm::mat4 view = camera.GetViewMatrix();
        lightingShader.setMat4("projection", projection);
        lightingShader.setMat4("view", view);

        // model for terrain
        glm::mat4 model = glm::mat4(1.0f);
        lightingShader.setMat4("model", model);

        // terrain material
        lightingShader.setVec3("material.diffuse", 0.2f, 0.7f, 0.2f);
        lightingShader.setVec3("material.specular", 0.2f, 0.2f, 0.2f);
        lightingShader.setFloat("material.shininess", 32.0f);

        // draw terrain
        glBindVertexArray(terrainVAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)terrainIndexCount, GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // cleanup
    glDeleteVertexArrays(1, &terrainVAO);
    glDeleteBuffers(1, &terrainVBO);
    glDeleteBuffers(1, &terrainEBO);

    glfwTerminate();
    return 0;
}


// --- terrain generator -----------------------------------------------------
void generateTerrain(std::vector<float>& vertices, std::vector<unsigned int>& indices, int N, float scale, float offsetX, float offsetZ, float amplitude, float freq)
{
    vertices.clear();
    indices.clear();
    vertices.reserve(N * N * 8);
    indices.reserve((N - 1) * (N - 1) * 6);

    // heights grid
    std::vector<float> heights(N * N);
    for (int z = 0; z < N; ++z)
    {
        for (int x = 0; x < N; ++x)
        {
            float wx = (float)x;
            float wz = (float)z;
            float h = sampleHeight(wx * scale, wz * scale, offsetX, offsetZ, amplitude, freq);
            heights[z * N + x] = h;
        }
    }

    // build vertices with normals computed via central differences
    for (int z = 0; z < N; ++z)
    {
        for (int x = 0; x < N; ++x)
        {
            float px = (x - N/2) * scale; // center grid around origin
            float pz = (z - N/2) * scale;
            float py = heights[z * N + x];

            // compute normals by sampling neighbors
            float hl = (x > 0) ? heights[z * N + (x - 1)] : heights[z * N + x];
            float hr = (x < N-1) ? heights[z * N + (x + 1)] : heights[z * N + x];
            float hd = (z > 0) ? heights[(z - 1) * N + x] : heights[z * N + x];
            float hu = (z < N-1) ? heights[(z + 1) * N + x] : heights[z * N + x];

            glm::vec3 normal;
            normal.x = hl - hr; // -dh/dx approx
            normal.y = 2.0f * scale; // arbitrary to keep y positive
            normal.z = hd - hu; // -dh/dz approx
            normal = glm::normalize(normal);

            // texcoords
            float u = (float)x / (N - 1);
            float v = (float)z / (N - 1);

            // push: pos(3), normal(3), tex(2)
            vertices.push_back(px);
            vertices.push_back(py);
            vertices.push_back(pz);
            vertices.push_back(normal.x);
            vertices.push_back(normal.y);
            vertices.push_back(normal.z);
            vertices.push_back(u);
            vertices.push_back(v);
        }
    }

    // indices (two triangles per quad)
    for (int z = 0; z < N - 1; ++z)
    {
        for (int x = 0; x < N - 1; ++x)
        {
            unsigned int i0 = z * N + x;
            unsigned int i1 = z * N + (x + 1);
            unsigned int i2 = (z + 1) * N + x;
            unsigned int i3 = (z + 1) * N + (x + 1);

            // triangle 1
            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);
            // triangle 2
            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }
}

float sampleHeight(float x, float z, float offsetX, float offsetZ, float amplitude, float freq)
{
    x += offsetX;
    z += offsetZ;

    int octaves = 6;
    float persistence = 0.5f;
    float lacunarity = 2.0f;

    float height = 0.0f;
    float amp = 1.0f; // start with 1, scale later
    float f = freq;

    for (int i = 0; i < octaves; ++i)
    {
        float n = stb_perlin_noise3(x * f, 0.0f, z * f, 0, 0, 0); // [-1,1]
        height += n * amp;
        amp *= persistence;
        f *= lacunarity;
    }

    // normalize to [0,1]
    height = (height + 1.0f) / 2.0f;

    // non-linear shaping: exaggerate peaks
    height = pow(height, 1.5f); // >1 → taller mountains, <1 → flatter

    return height * amplitude;
}


// process input
void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);

    // move terrain patch around (these modify the sample offsets used by height function)
    float moveSpeed = 20.0f * (terrainAmplitude / 10.0f); // scale speed by height if you want
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        terrainOffsetZ -= moveSpeed * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        terrainOffsetZ += moveSpeed * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        terrainOffsetX -= moveSpeed * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        terrainOffsetX += moveSpeed * deltaTime;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;

    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

unsigned int loadTexture(char const * path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}
