/**
 * @file diffuse.cpp
 * Defines a diffuse reflection.
 *
 * @author Ricardo Dutra da Silva
 */

#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/geometric.hpp"
#include "utils.h"
#include <GL/freeglut.h>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

/* Globals */
/** Window width. */
int win_width = 600;
/** Window height. */
int win_height = 600;

/** Program variable. */
int program;
/** Vertex array object. */
unsigned int VAO;
/** Vertex buffer object. */
unsigned int VBO;

struct Node {
    glm::vec3 position;

    int connected_to;
    bool in_tree;
    float cost;
};

std::vector<Node> nodes;
std::vector<int> not_included;
int last_added = -1;

glm::vec3 camera_pos = glm::vec3(0.0f, 15.0f, 10.0f);

/** Vertex shader. */
const char *vertex_code = "\n"
                          "#version 330 core\n"
                          "layout (location = 0) in vec3 position;\n"
                          "layout (location = 1) in vec3 normal;\n"
                          "\n"
                          "uniform mat4 model;\n"
                          "uniform mat4 view;\n"
                          "uniform mat4 projection;\n"
                          "\n"
                          "out vec3 vNormal;\n"
                          "out vec3 fragPosition;\n"
                          "\n"
                          "void main()\n"
                          "{\n"
                          "    gl_Position = projection * view * model * vec4(position, 1.0);\n"
                          "    vNormal = mat3(transpose(inverse(model)))*normal;\n"
                          "    fragPosition = vec3(model * vec4(position, 1.0));\n"
                          "}\0";

/*"    vNormal = mat3(transpose(inverse(model)))*normal;\n"*/

/** Fragment shader. */
const char *fragment_code = "\n"
                            "#version 330 core\n"
                            "\n"
                            "in vec3 vNormal;\n"
                            "in vec3 fragPosition;\n"
                            "\n"
                            "out vec4 fragColor;\n"
                            "\n"
                            "uniform vec3 objectColor;\n"
                            "uniform vec3 lightColor;\n"
                            "uniform vec3 lightDirection;\n"
                            "\n"
                            "void main()\n"
                            "{\n"
                            "    float kd = 0.8;\n"
                            "    vec3 n = normalize(vNormal);\n"
                            "    vec3 l = normalize(lightDirection);\n"
                            "\n"
                            "    float diff = 0.6 * max(dot(n,-l) + 1.0, 0.0);\n"
                            "    vec3 diffuse = kd * diff * lightColor;\n"
                            "\n"
                            "    vec3 light = diffuse * objectColor;\n"
                            "    fragColor = vec4(light, 1.0);\n"
                            "}\0";

/* Functions. */
void display(void);
void reshape(int, int);
void keyboard(unsigned char, int, int);
void initData(void);
void initShaders(void);
void runPrimStep();

/**
 * Drawing function.
 *
 * Draws primitive.
 */
void display() {
    glClearColor(0.3, 0.6, 0.8, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(program);
    glBindVertexArray(VAO);

    unsigned int loc;

    glm::mat4 view =
        glm::lookAt(camera_pos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    loc = glGetUniformLocation(program, "view");
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(view));

    glm::mat4 projection =
        glm::perspective(glm::radians(45.0f), (win_width / (float)win_height), 0.1f, 100.0f);
    loc = glGetUniformLocation(program, "projection");
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(projection));

    // Light color.
    loc = glGetUniformLocation(program, "lightColor");
    glUniform3f(loc, 1.0, 1.0, 1.0);

    // Light position.
    loc = glGetUniformLocation(program, "lightDirection");
    glUniform3f(loc, -1.0, -3.0, -2.0);

    // draw edges
    for (int i = 0; i < nodes.size(); i++) {
        auto node = nodes[i];
        if (!node.in_tree || node.connected_to == -1)
            continue;

        // Object color.
        loc = glGetUniformLocation(program, "objectColor");
        if (i == last_added) {
            glUniform3f(loc, 0.0, 1.0, 0.0);
        } else {
            glUniform3f(loc, 1.0, 1.0, 1.0);
        }

        auto start = node.position;
        auto end = nodes[node.connected_to].position;

        float dist = glm::distance(start, end);

        glm::vec3 da = glm::normalize(glm::vec3(0.0f, 0.0f, 1.0f));
        glm::vec3 db = glm::normalize(end - start);

        auto dir = end - start;
        float angle = atan2(dir.x, dir.z);

        auto model = glm::mat4(1.0f);
        model = glm::translate(model, (start + end) / 2.0f);
        model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(0.15f, 0.15f, dist));
        /* model = glm::scale(model, glm::vec3(0.25f, 0.25f, 0.5f)); */

        unsigned int loc = glGetUniformLocation(program, "model");
        glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(model));

        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // Object color.
    loc = glGetUniformLocation(program, "objectColor");
    glUniform3f(loc, 1.0, 0.1, 0.1);

    // draw nodes
    for (auto node : nodes) {
        auto model = glm::mat4(1.0f);
        model = glm::translate(model, node.position);
        model = glm::scale(model, glm::vec3(0.5));

        unsigned int loc = glGetUniformLocation(program, "model");
        glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(model));

        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    glutSwapBuffers();
}

/**
 * Reshape function.
 *
 * Called when window is resized.
 *
 * @param width New window width.
 * @param height New window height.
 */
void reshape(int width, int height) {
    win_width = width;
    win_height = height;
    glViewport(0, 0, width, height);
    glutPostRedisplay();
}

/**
 * Keyboard function.
 *
 * Called to treat pressed keys.
 *
 * @param key Pressed key.
 * @param x Mouse x coordinate when key pressed.
 * @param y Mouse y coordinate when key pressed.
 */
void keyboard(unsigned char key, int x, int y) {
    glutPostRedisplay();

    switch (key) {
    case 27:
        break;
        glutLeaveMainLoop();
    case 'q':
    case 'Q':
        glutLeaveMainLoop();
        break;
    case 'w':
        camera_pos.y += 0.5f;
        break;
    case 's':
        camera_pos.y -= 0.5f;
        break;
    case 'a':
        camera_pos.x -= 0.5f;
        break;
    case 'd':
        camera_pos.x += 0.5f;
        break;
    case 'n':
        runPrimStep();
        break;
    }
}

/**
 * Init vertex data.
 *
 * Defines the coordinates for vertices, creates the arrays for OpenGL.
 */
void initData() {
    // Set cube vertices.
    float vertices[] = {
        // coordinate      // normal
        -0.5f, -0.5f, 0.5f,  0.0f,  0.0f,  1.0f,  0.5f,  -0.5f, 0.5f,  0.0f,  0.0f,  1.0f,  0.5f,
        0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  -0.5f, -0.5f, 0.5f,  0.0f,  0.0f,  1.0f,  0.5f,  0.5f,
        0.5f,  0.0f,  0.0f,  1.0f,  -0.5f, 0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.5f,  -0.5f, 0.5f,
        1.0f,  0.0f,  0.0f,  0.5f,  -0.5f, -0.5f, 1.0f,  0.0f,  0.0f,  0.5f,  0.5f,  -0.5f, 1.0f,
        0.0f,  0.0f,  0.5f,  -0.5f, 0.5f,  1.0f,  0.0f,  0.0f,  0.5f,  0.5f,  -0.5f, 1.0f,  0.0f,
        0.0f,  0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.5f,  -0.5f, -0.5f, 0.0f,  0.0f,  -1.0f,
        -0.5f, -0.5f, -0.5f, 0.0f,  0.0f,  -1.0f, -0.5f, 0.5f,  -0.5f, 0.0f,  0.0f,  -1.0f, 0.5f,
        -0.5f, -0.5f, 0.0f,  0.0f,  -1.0f, -0.5f, 0.5f,  -0.5f, 0.0f,  0.0f,  -1.0f, 0.5f,  0.5f,
        -0.5f, 0.0f,  0.0f,  -1.0f, -0.5f, -0.5f, -0.5f, -1.0f, 0.0f,  0.0f,  -0.5f, -0.5f, 0.5f,
        -1.0f, 0.0f,  0.0f,  -0.5f, 0.5f,  0.5f,  -1.0f, 0.0f,  0.0f,  -0.5f, -0.5f, -0.5f, -1.0f,
        0.0f,  0.0f,  -0.5f, 0.5f,  0.5f,  -1.0f, 0.0f,  0.0f,  -0.5f, 0.5f,  -0.5f, -1.0f, 0.0f,
        0.0f,  -0.5f, 0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        0.5f,  0.5f,  -0.5f, 0.0f,  1.0f,  0.0f,  -0.5f, 0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.5f,
        0.5f,  -0.5f, 0.0f,  1.0f,  0.0f,  -0.5f, 0.5f,  -0.5f, 0.0f,  1.0f,  0.0f,  -0.5f, -0.5f,
        0.5f,  0.0f,  -1.0f, 0.0f,  -0.5f, -0.5f, -0.5f, 0.0f,  -1.0f, 0.0f,  0.5f,  -0.5f, 0.5f,
        0.0f,  -1.0f, 0.0f,  -0.5f, -0.5f, -0.5f, 0.0f,  -1.0f, 0.0f,  0.5f,  -0.5f, -0.5f, 0.0f,
        -1.0f, 0.0f,  0.5f,  -0.5f, 0.5f,  0.0f,  -1.0f, 0.0f};

    // Vertex array.
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // Vertex buffer
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Set attributes.
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Unbind Vertex Array Object.
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
}

/** Create program (shaders).
 *
 * Compile shaders and create the program.
 */
void initShaders() {
    // Request a program and shader slots from GPU
    program = createShaderProgram(vertex_code, fragment_code);
}

void initGraph() {
    nodes = std::vector<Node>();
    for (int i = 0; i < 25; i++) {
        int x = i / 5;
        int y = i % 5;
        float dx = 0.5 * ((float)rand() / (float)(RAND_MAX)-1.0);
        float dy = 0.5 * ((float)rand() / (float)(RAND_MAX)-1.0);
        /* float dx = 0.0; */
        /* float dy = 0.0; */

        auto position = glm::vec3((float)x * 2.0f - 4.0f + dx, 0.0f, -4.0f + 2.0f * (float)y + dy);
        nodes.push_back(
            Node{.position = position, .connected_to = -1, .in_tree = false, .cost = 1.0f / 0.0f});
    }
    for (int v = 0; v < nodes.size(); v++) {
        not_included.push_back(v);
    }
}

/// https://en.wikipedia.org/wiki/Prim%27s_algorithm#Description
void runPrimStep() {

    if (!not_included.empty()) {
        float min_cost = 1.0f / 0.0f;
        int min = -1;
        for (int i = 0; i < not_included.size(); i++) {
            int v = not_included[i];
            if (min == -1 || nodes[v].cost < min_cost) {
                min_cost = nodes[v].cost;
                min = i;
            }
        }
        int v = not_included[min];
        not_included.erase(not_included.begin() + min);
        nodes[v].in_tree = true;
        last_added = v;

        for (int w : not_included) {
            float new_cost = glm::distance(nodes[v].position, nodes[w].position);
            if (new_cost < nodes[w].cost) {
                nodes[w].connected_to = v;
                nodes[w].cost = new_cost;
            }
        }
    } else {
        last_added = -1;
    }
}

int main(int argc, char **argv) {
    glutInit(&argc, argv);
    glutInitContextVersion(3, 3);
    glutInitContextProfile(GLUT_CORE_PROFILE);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(win_width, win_height);
    glutCreateWindow(argv[0]);
    glewInit();

    // Init the nodes of the graph
    initGraph();
    runPrimStep();

    // Init vertex data for the triangle.
    initData();

    // Create shaders.
    initShaders();

    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);

    glutMainLoop();
}
