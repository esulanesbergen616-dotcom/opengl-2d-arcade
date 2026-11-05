#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>

// эксперименты из методички (по умолчанию - правильные значения)
#define PITCH_CLAMP 1   // 0 - без ограничения ±89°: смотрим строго вверх -> NaN, сцена пропадает
#define INVERT_Y    0   // 1 - yoffset = ypos - lastY: камера поворачивается наоборот

// --- камера (9-я неделя) ---
glm::vec3 camPos   = glm::vec3(0.0f, 0.0f,  3.0f);
glm::vec3 camFront = glm::vec3(0.0f, 0.0f, -1.0f);   // начальное значение временное, дальше - из yaw/pitch
glm::vec3 camUp    = glm::vec3(0.0f, 1.0f,  0.0f);

// --- мышь (10-я неделя) ---
float yaw   = -90.0f;   // yaw = 0 -> (1, 0, 0), а камера должна смотреть в -Z
float pitch =   0.0f;
float lastX = 400.0f;   // центр окна
float lastY = 300.0f;
bool  firstMouse = true;
float sensitivity = 0.1f;    // градусов на пиксель; меняется клавишами - / = во время работы

// Режим мыши (клавиша M):
//  false - как в методичке: GLFW_CURSOR_DISABLED, курсор скрыт и держится в центре окна;
//  true  - "перетаскивание": курсор обычный, камера крутится, пока зажата левая кнопка.
// В WSL (WSLg) GLFW не может вернуть курсор в центр окна, поэтому смещение каждый раз
// считается от центра и накапливается -> камера крутится по кругу. Там включаем перетаскивание.
bool dragMode = std::getenv("WSL_DISTRO_NAME") != nullptr;

void applyMouseMode(GLFWwindow* window) {
    glfwSetInputMode(window, GLFW_CURSOR, dragMode ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    firstMouse = true;
}

// --- зум (10-я неделя) ---
float fov = 45.0f;

int gWidth  = 800;
int gHeight = 600;

void updateTitle(GLFWwindow* window) {
    char title[128];
    std::snprintf(title, sizeof(title),
                  "Week 10 - %s (M)  sensitivity %.3f (-/=)  fov %.0f  |  R - reset",
                  dragMode ? "drag: hold LMB" : "FPS mouse", sensitivity, fov);
    glfwSetWindowTitle(window, title);
}

void resetCamera() {
    camPos   = glm::vec3(0.0f, 0.0f,  3.0f);
    camFront = glm::vec3(0.0f, 0.0f, -1.0f);
    yaw   = -90.0f;
    pitch =   0.0f;
    fov   =  45.0f;
}

void framebuffer_size_callback(GLFWwindow*, int width, int height) {
    gWidth  = width;
    gHeight = height;
    glViewport(0, 0, width, height);
}

// одиночные нажатия (не каждый кадр, как в processInput)
void key_callback(GLFWwindow* window, int key, int, int action, int) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    if (key == GLFW_KEY_MINUS) sensitivity *= 0.8f;
    if (key == GLFW_KEY_EQUAL) sensitivity *= 1.25f;
    if (sensitivity < 0.005f) sensitivity = 0.005f;
    if (sensitivity > 0.5f)   sensitivity = 0.5f;
    if (key == GLFW_KEY_R && action == GLFW_PRESS) resetCamera();
    if (key == GLFW_KEY_M && action == GLFW_PRESS) { dragMode = !dragMode; applyMouseMode(window); }
    updateTitle(window);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    // режим перетаскивания: без зажатой ЛКМ только запоминаем позицию, чтобы при нажатии не было рывка
    if (dragMode && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) != GLFW_PRESS) {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
        return;
    }

    // первый вызов: lastX/lastY ещё выдуманные, иначе камеру "швырнёт"
    if (firstMouse) {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
    }

    float xoffset = (float)xpos - lastX;
#if INVERT_Y
    float yoffset = (float)ypos - lastY;
#else
    float yoffset = lastY - (float)ypos;   // экранный y растёт вниз, pitch - вверх
#endif
    lastX = (float)xpos;
    lastY = (float)ypos;

    // смещение мыши - в пикселях, на dt НЕ умножаем
    yaw   += xoffset * sensitivity;
    pitch += yoffset * sensitivity;

#if PITCH_CLAMP
    if (pitch >  89.0f) pitch =  89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
#endif

    glm::vec3 dir;
    dir.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    dir.y = sin(glm::radians(pitch));
    dir.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    camFront = glm::normalize(dir);
}

void scroll_callback(GLFWwindow* window, double, double yoffset) {
    fov -= (float)yoffset;   // колесо вперёд -> меньше угол обзора -> приближение
    if (fov <  1.0f) fov =  1.0f;
    if (fov > 45.0f) fov = 45.0f;
    updateTitle(window);
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
}

unsigned int makeShader(const char* vsSrc, const char* fsSrc) {
    int ok;
    char log[1024];

    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsSrc, nullptr);
    glCompileShader(vs);
    glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
    if (!ok) { glGetShaderInfoLog(vs, 1024, nullptr, log); std::cerr << log << "\n"; }

    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsSrc, nullptr);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    if (!ok) { glGetShaderInfoLog(fs, 1024, nullptr, log); std::cerr << log << "\n"; }

    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) { glGetProgramInfoLog(prog, 1024, nullptr, log); std::cerr << log << "\n"; }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

const char* vertexSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vColor;

void main() {
    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
    vColor = aColor;
}
)";

const char* fragmentSrc = R"(
#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() { FragColor = vec4(vColor, 1.0); }
)";

// 6 граней x 2 треугольника x 3 вершины = 36 вершин, (x y z  r g b), порядок обхода CCW снаружи
float cubeVertices[] = {
    // перед (z = +0.5) - красный
    -0.5f, -0.5f,  0.5f,   1.0f,  0.0f,  0.0f,
     0.5f, -0.5f,  0.5f,   1.0f,  0.0f,  0.0f,
     0.5f,  0.5f,  0.5f,   1.0f,  0.0f,  0.0f,
     0.5f,  0.5f,  0.5f,   1.0f,  0.0f,  0.0f,
    -0.5f,  0.5f,  0.5f,   1.0f,  0.0f,  0.0f,
    -0.5f, -0.5f,  0.5f,   1.0f,  0.0f,  0.0f,
    // зад (z = -0.5) - зелёный
    -0.5f, -0.5f, -0.5f,   0.0f,  1.0f,  0.0f,
    -0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,
     0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,
     0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,
     0.5f, -0.5f, -0.5f,   0.0f,  1.0f,  0.0f,
    -0.5f, -0.5f, -0.5f,   0.0f,  1.0f,  0.0f,
    // лево (x = -0.5) - синий
    -0.5f, -0.5f, -0.5f,   0.0f,  0.0f,  1.0f,
    -0.5f, -0.5f,  0.5f,   0.0f,  0.0f,  1.0f,
    -0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,
    -0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,
    -0.5f,  0.5f, -0.5f,   0.0f,  0.0f,  1.0f,
    -0.5f, -0.5f, -0.5f,   0.0f,  0.0f,  1.0f,
    // право (x = +0.5) - жёлтый
     0.5f, -0.5f, -0.5f,   1.0f,  1.0f,  0.0f,
     0.5f,  0.5f, -0.5f,   1.0f,  1.0f,  0.0f,
     0.5f,  0.5f,  0.5f,   1.0f,  1.0f,  0.0f,
     0.5f,  0.5f,  0.5f,   1.0f,  1.0f,  0.0f,
     0.5f, -0.5f,  0.5f,   1.0f,  1.0f,  0.0f,
     0.5f, -0.5f, -0.5f,   1.0f,  1.0f,  0.0f,
    // низ (y = -0.5) - пурпурный
    -0.5f, -0.5f, -0.5f,   1.0f,  0.0f,  1.0f,
     0.5f, -0.5f, -0.5f,   1.0f,  0.0f,  1.0f,
     0.5f, -0.5f,  0.5f,   1.0f,  0.0f,  1.0f,
     0.5f, -0.5f,  0.5f,   1.0f,  0.0f,  1.0f,
    -0.5f, -0.5f,  0.5f,   1.0f,  0.0f,  1.0f,
    -0.5f, -0.5f, -0.5f,   1.0f,  0.0f,  1.0f,
    // верх (y = +0.5) - голубой
    -0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  1.0f,
    -0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  1.0f,
     0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  1.0f,
     0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  1.0f,
     0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  1.0f,
    -0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  1.0f,
};

// исходные 5 кубов из 9-й недели
const glm::vec3 cubePos[5] = {
    { 0.0f,  0.0f,  0.0f},
    { 2.0f,  0.5f, -2.0f},
    {-1.8f, -0.3f, -1.0f},
    { 1.2f, -0.8f, -3.5f},
    {-2.2f,  1.0f, -4.0f},
};

// --- ориентиры: пол-сетка, кольцо кубов вокруг, прицел ---
const float FLOOR_Y   = -1.5f;
const int   GRID_HALF = 20;      // сетка от -20 до +20
const int   RING_N    = 12;      // кубов по кругу
const float RING_R    = 10.0f;   // радиус круга

// линии сетки: (x y z  r g b), оси X (красная) и Z (синяя) выделены
std::vector<float> makeGrid() {
    std::vector<float> v;
    auto line = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c) {
        v.insert(v.end(), {a.x, a.y, a.z, c.r, c.g, c.b, b.x, b.y, b.z, c.r, c.g, c.b});
    };
    float h = (float)GRID_HALF;
    for (int i = -GRID_HALF; i <= GRID_HALF; i++) {
        float t = (float)i;
        glm::vec3 c = (i % 5 == 0) ? glm::vec3(0.45f) : glm::vec3(0.25f);
        line({t, FLOOR_Y, -h}, {t, FLOOR_Y, h}, i == 0 ? glm::vec3(0.2f, 0.4f, 1.0f) : c);  // вдоль Z
        line({-h, FLOOR_Y, t}, {h, FLOOR_Y, t}, i == 0 ? glm::vec3(1.0f, 0.3f, 0.3f) : c);  // вдоль X
    }
    return v;
}

// прицел в центре экрана (в NDC, белый)
float crosshair[] = {
    -0.03f, 0.0f, 0.0f,   1.0f, 1.0f, 1.0f,
     0.03f, 0.0f, 0.0f,   1.0f, 1.0f, 1.0f,
     0.0f, -0.03f, 0.0f,  1.0f, 1.0f, 1.0f,
     0.0f,  0.03f, 0.0f,  1.0f, 1.0f, 1.0f,
};

unsigned int makeVao(const float* data, size_t bytes, unsigned int& vbo) {
    unsigned int vao;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, bytes, data, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    return vao;
}

int main() {
    if (!glfwInit()) {
        std::cerr << "GLFW init failed\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(gWidth, gHeight, "Week 10 - Mouse & Free Camera", nullptr, nullptr);
    if (!window) {
        std::cerr << "Window creation failed\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSwapInterval(1);

    // FPS-режим: курсор скрыт и не выходит из окна (GLFW_CURSOR_DISABLED); в WSL - перетаскивание
    applyMouseMode(window);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);
    // "сырое" движение мыши без системного ускорения - поворот ровнее и предсказуемее
    if (glfwRawMouseMotionSupported())
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    updateTitle(window);

    if (gladLoadGL(glfwGetProcAddress) == 0) {
        std::cerr << "GLAD load failed\n";
        glfwTerminate();
        return -1;
    }

    // на HiDPI размер фреймбуфера может отличаться от размера окна
    glfwGetFramebufferSize(window, &gWidth, &gHeight);
    glViewport(0, 0, gWidth, gHeight);

    unsigned int shader = makeShader(vertexSrc, fragmentSrc);

    unsigned int vbo, gridVbo, crossVbo;
    unsigned int vao = makeVao(cubeVertices, sizeof(cubeVertices), vbo);

    std::vector<float> grid = makeGrid();
    int gridVerts = (int)(grid.size() / 6);
    unsigned int gridVao  = makeVao(grid.data(), grid.size() * sizeof(float), gridVbo);
    unsigned int crossVao = makeVao(crosshair, sizeof(crosshair), crossVbo);

    // location'ы - один раз, до цикла
    int locModel = glGetUniformLocation(shader, "uModel");
    int locView  = glGetUniformLocation(shader, "uView");
    int locProj  = glGetUniformLocation(shader, "uProjection");

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glEnable(GL_DEPTH_TEST);

    float lastFrame = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        processInput(window);

        float currentFrame = (float)glfwGetTime();
        float dt = currentFrame - lastFrame;
        lastFrame = currentFrame;

        float speed = 2.5f * dt;

        // WASD без изменений, но camFront теперь поворачивается мышью -> идём туда, куда смотрим
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camPos += speed * camFront;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camPos -= speed * camFront;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camPos -= glm::normalize(glm::cross(camFront, camUp)) * speed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camPos += glm::normalize(glm::cross(camFront, camUp)) * speed;

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shader);

        float aspect = (float)gWidth / (float)gHeight;
        float angle  = (float)glfwGetTime();

        // projection и view - один раз за кадр; fov - от колеса, camFront - от мыши
        glm::mat4 projection = glm::perspective(glm::radians(fov), aspect, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(camPos, camPos + camFront, camUp);
        glUniformMatrix4fv(locView, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(locProj, 1, GL_FALSE, glm::value_ptr(projection));

        glBindVertexArray(vao);
        for (int i = 0; i < 5; i++) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, cubePos[i]);
            model = glm::rotate(model, angle * (0.5f + i * 0.3f), glm::vec3(0.3f, 1.0f, 0.0f));
            glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(model));
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // кольцо кубов вокруг начала координат - куда ни повернись, что-то видно
        for (int i = 0; i < RING_N; i++) {
            float a = glm::radians(360.0f / RING_N * i);
            glm::vec3 p(RING_R * cos(a), FLOOR_Y + 0.5f + (i % 3), RING_R * sin(a));
            glm::mat4 model = glm::translate(glm::mat4(1.0f), p);
            model = glm::rotate(model, angle * 0.4f, glm::vec3(0.0f, 1.0f, 0.0f));
            glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(model));
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // пол-сетка
        glm::mat4 identity(1.0f);
        glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(identity));
        glBindVertexArray(gridVao);
        glDrawArrays(GL_LINES, 0, gridVerts);

        // прицел: сразу в NDC, без view/projection; по X сжимаем на aspect, чтобы крест был квадратным
        glm::mat4 crossModel = glm::scale(identity, glm::vec3(1.0f / aspect, 1.0f, 1.0f));
        glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(crossModel));
        glUniformMatrix4fv(locView,  1, GL_FALSE, glm::value_ptr(identity));
        glUniformMatrix4fv(locProj,  1, GL_FALSE, glm::value_ptr(identity));
        glDisable(GL_DEPTH_TEST);
        glBindVertexArray(crossVao);
        glDrawArrays(GL_LINES, 0, 4);
        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &vao);
    glDeleteVertexArrays(1, &gridVao);
    glDeleteVertexArrays(1, &crossVao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &gridVbo);
    glDeleteBuffers(1, &crossVbo);
    glDeleteProgram(shader);

    glfwTerminate();
    return 0;
}
