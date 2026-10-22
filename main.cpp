#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <iostream>
#include <vector>

// Талап етілген ажыратымдылық
const int WIDTH  = 1280;
const int HEIGHT = 960;

// Ойын күйлері мен ұпай
bool prevSpaceState = false;
int score = 0;
bool gameOver = false;

// Экран өлшемі өзгергенде viewport-ты реттеу
void onResize(GLFWwindow*, int width, int height) {
    glViewport(0, 0, width, height);
}

// Шейдерлерді компиляциялау функциясы
unsigned int makeShader(const char* vsSrc, const char* fsSrc) {
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsSrc, nullptr);
    glCompileShader(vs);
    int ok;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(vs, 1024, nullptr, log);
        std::cerr << "Vertex Shader қатесі:\n" << log << "\n";
    }

    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsSrc, nullptr);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(fs, 1024, nullptr, log);
        std::cerr << "Fragment Shader қатесі:\n" << log << "\n";
    }

    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, 1024, nullptr, log);
        std::cerr << "Shader Program линковка қатесі:\n" << log << "\n";
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

const char* vertexShaderSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

uniform mat4 uModel;
out vec3 vColor;

void main() {
    gl_Position = uModel * vec4(aPos, 1.0);
    vColor = aColor;
}
)";

const char* fragmentShaderSrc = R"(
#version 330 core
in vec3 vColor;
out vec4 FragColor;

void main() {
    FragColor = vec4(vColor, 1.0);
}
)";

int main() {
    if (!glfwInit()) {
        std::cerr << "GLFW іске қосылмады!\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "2D Arcade - Spikey Wheel Obstacle", nullptr, nullptr);
    if (!window) {
        std::cerr << "OpenGL терезесі жасалмады!\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, onResize);
    glfwSwapInterval(1);

    if (gladLoadGL(glfwGetProcAddress) == 0) {
        std::cerr << "GLAD жүктелмеді!\n";
        glfwTerminate();
        return -1;
    }

    std::cout << "=== Ойын іске қосылды: 8 бұрышты тісті кедергі! ===\n";
    std::cout << "Басқару: Space - Секіру, ESC - Шығу.\n";

    const int STRIDE = 6 * sizeof(float);

    // 1. ШАРДЫҢ ГЕОМЕТРИЯСЫ (EBO: Центр + 48 нүкте)
    std::vector<float> circleVertices;
    std::vector<unsigned int> circleIndices;
    float radius = 0.10f; 
    int segments = 48;    

    circleVertices.push_back(0.0f); circleVertices.push_back(0.0f); circleVertices.push_back(0.0f);
    circleVertices.push_back(0.2f); circleVertices.push_back(0.9f); circleVertices.push_back(1.0f); 

    for (int i = 0; i < segments; ++i) {
        float angle = 2.0f * 3.14159265f * float(i) / float(segments);
        float x = radius * std::cos(angle);
        float y = radius * std::sin(angle);

        circleVertices.push_back(x); circleVertices.push_back(y); circleVertices.push_back(0.0f);
        circleVertices.push_back(0.0f); circleVertices.push_back(0.4f); circleVertices.push_back(0.8f); 
    }

    for (int i = 0; i < segments; ++i) {
        circleIndices.push_back(0);
        circleIndices.push_back(i + 1);
        circleIndices.push_back(((i + 1) % segments) + 1);
    }

    unsigned int circleVAO, circleVBO, circleEBO;
    glGenVertexArrays(1, &circleVAO);
    glGenBuffers(1, &circleVBO);
    glGenBuffers(1, &circleEBO);

    glBindVertexArray(circleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
    glBufferData(GL_ARRAY_BUFFER, circleVertices.size() * sizeof(float), circleVertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, circleEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, circleIndices.size() * sizeof(unsigned int), circleIndices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, STRIDE, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, STRIDE, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // 2. 8 БҰРЫШТЫ ТІСТІ КЕДЕРГІ (SPIKEY WHEEL) ЖЕОМЕТРИЯСЫ
    std::vector<float> spikeVertices;
    std::vector<unsigned int> spikeIndices;
    float outerR = 0.13f; // Тістердің ұшы
    float innerR = 0.07f; // Ойықтар
    int spikes = 8;
    int totalPoints = spikes * 2; // 16 нүкте

    // Центр (Индекс 0) - Қызыл түсті орталық
    spikeVertices.push_back(0.0f); spikeVertices.push_back(0.0f); spikeVertices.push_back(0.0f);
    spikeVertices.push_back(1.0f); spikeVertices.push_back(0.2f); spikeVertices.push_back(0.1f);

    for (int i = 0; i < totalPoints; ++i) {
        float angle = 2.0f * 3.14159265f * float(i) / float(totalPoints);
        float r = (i % 2 == 0) ? outerR : innerR;
        float x = r * std::cos(angle);
        float y = r * std::sin(angle);

        spikeVertices.push_back(x); spikeVertices.push_back(y); spikeVertices.push_back(0.0f);
        // Отты-қызғылт градиент түстер
        spikeVertices.push_back(1.0f); spikeVertices.push_back(0.6f); spikeVertices.push_back(0.0f);
    }

    for (int i = 1; i <= totalPoints; ++i) {
        spikeIndices.push_back(0);
        spikeIndices.push_back(i);
        spikeIndices.push_back((i % totalPoints) + 1);
    }

    unsigned int spikeVAO, spikeVBO, spikeEBO;
    glGenVertexArrays(1, &spikeVAO);
    glGenBuffers(1, &spikeVBO);
    glGenBuffers(1, &spikeEBO);

    glBindVertexArray(spikeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, spikeVBO);
    glBufferData(GL_ARRAY_BUFFER, spikeVertices.size() * sizeof(float), spikeVertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, spikeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, spikeIndices.size() * sizeof(unsigned int), spikeIndices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, STRIDE, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, STRIDE, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    unsigned int shaderProgram = makeShader(vertexShaderSrc, fragmentShaderSrc);
    int modelLoc = glGetUniformLocation(shaderProgram, "uModel");

    // Физика және ойын параметрлері
    float ballX = -0.5f;          
    float ballBaseY = -0.55f;    
    float ballY = ballBaseY;
    float ballVelocityY = 0.0f;
    float gravity = -5.0f;       
    float jumpForce = 2.3f;      
    bool isGrounded = true;

    float spikeX = 1.5f;        
    float spikeY = ballBaseY;    
    float spikeSpeed = 1.3f;     
    float spikeRotationAngle = 0.0f; // Айналу бұрышы

    float lastRealTime = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        float currentRealTime = (float)glfwGetTime();
        float dt = currentRealTime - lastRealTime;
        lastRealTime = currentRealTime;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        // Space басылғанда секіру
        bool spacePressed = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);
        if (spacePressed && !prevSpaceState && isGrounded && !gameOver) {
            ballVelocityY = jumpForce;
            isGrounded = false;
        }
        prevSpaceState = spacePressed;

        if (!gameOver) {
            // Шар физикасы
            ballVelocityY += gravity * dt;
            ballY += ballVelocityY * dt;

            if (ballY <= ballBaseY) {
                ballY = ballBaseY;
                ballVelocityY = 0.0f;
                isGrounded = true;
            }

            // Тісті дөңгелектің оңнан солға жылжуы және айналуы
            spikeX -= spikeSpeed * dt;
            spikeRotationAngle += 3.0f * dt; // Үздіксіз айналу жылдамдығы

            if (spikeX < -1.5f) {
                spikeX = 1.5f;
                score++;
                std::cout << "Ұпай: " << score << "\n";
            }

            // Қақтығысты тексеру (Шар жарылу логикасы)
            float distX = std::abs(ballX - spikeX);
            float distY = std::abs(ballY - spikeY);
            float distance = std::sqrt(distX * distX + distY * distY);
            
            if (distance < (radius + outerR * 0.75f)) {
                gameOver = true;
                std::cout << "\n=== ШАР ЖАРЫЛДЫ! Тісті дөңгелекке тиіп кеттіңіз! ==\n";
                std::cout << "Жинаған ұпайыңыз: " << score << "\n";
                std::cout << "Шығу үшін ESC пернесін басыңыз.\n\n";
            }
        }

        // Экранды тазалау
        glClearColor(0.05f, 0.06f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // 1. Шарды салу
        glm::mat4 ballModel = glm::mat4(1.0f);
        ballModel = glm::translate(ballModel, glm::vec3(ballX, ballY, 0.0f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(ballModel));
        glBindVertexArray(circleVAO);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(circleIndices.size()), GL_UNSIGNED_INT, 0);

        // 2. 8 бұрышты айналатын тісті кедергіні салу
        glm::mat4 spikeModel = glm::mat4(1.0f);
        spikeModel = glm::translate(spikeModel, glm::vec3(spikeX, spikeY, 0.0f));
        spikeModel = glm::rotate(spikeModel, spikeRotationAngle, glm::vec3(0.0f, 0.0f, 1.0f)); // Өз осінен айналу
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(spikeModel));
        glBindVertexArray(spikeVAO);
        glDrawElements(GL_TRIANGLES, spikeIndices.size(), GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Ресурстарды тазалау
    glDeleteVertexArrays(1, &circleVAO); glDeleteBuffers(1, &circleVBO); glDeleteBuffers(1, &circleEBO);
    glDeleteVertexArrays(1, &spikeVAO); glDeleteBuffers(1, &spikeVBO); glDeleteBuffers(1, &spikeEBO);
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}