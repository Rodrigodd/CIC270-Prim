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
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

/* Globals */
/** Window width. */
int win_width = 600;
/** Window height. */
int win_height = 600;

/** Program variable. */
int program;

/// Modelo de uma casinha.
unsigned int VAO_CASA;
unsigned int VBO_CASA;

/// Modelo de um cubo.
unsigned int VAO_CUBO;
unsigned int VBO_CUBO;

/// Um nó no grafo
struct Node {
    /// A posição do grafo
    glm::vec3 position;

    /// Se esse nó já foi adicionado a árvore.
    bool in_tree;

    /// O indice do nó na árvore mais próximo deste.
    int connected_to;
    /// A distância ao nó na árvore mais próximo deste.
    float cost;
};

/// Todos os nós do grafo.
std::vector<Node> nodes;
/// Os nós ainda não incluídos na árvore mínima.
std::vector<int> not_included;
/// O indice do último nó adicionado à àrvore mínima.
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
void initGraph();

/**
 * Drawing function.
 *
 * Draws primitive.
 */
void display() {
    glClearColor(0.3, 0.6, 0.8, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(program);

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
    glBindVertexArray(VAO_CUBO);
    for (int i = 0; i < nodes.size(); i++) {
        auto node = nodes[i];
        if (!node.in_tree || node.connected_to == -1)
            continue;

        // Object color.
        loc = glGetUniformLocation(program, "objectColor");
        if (i == last_added) {
            glUniform3f(loc, 0.1, 0.1, 0.85);
        } else {
            glUniform3f(loc, 0.85, 0.7, 0.5);
        }

        auto start = node.position;
        auto end = nodes[node.connected_to].position;

        float dist = glm::distance(start, end);

        auto dir = end - start;
        float angle = atan2(dir.x, dir.z);

        auto model = glm::mat4(1.0f);
        model = glm::translate(model, (start + end) / 2.0f);
        model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(0.3f, 0.05f, dist));

        loc = glGetUniformLocation(program, "model");
        glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(model));

        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // draw ground
    {
        loc = glGetUniformLocation(program, "objectColor");
        glUniform3f(loc, 0.3, 0.8, 0.0);

        auto model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0, -0.5, 0.0));
        model = glm::scale(model, glm::vec3(12.0, 1.0, 12.0));

        loc = glGetUniformLocation(program, "model");
        glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(model));

        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // draw nodes
    glBindVertexArray(VAO_CASA);
    for (auto node : nodes) {
        loc = glGetUniformLocation(program, "objectColor");
        if (node.in_tree) {
            glUniform3f(loc, 1.0, 0.2, 0.2);
        } else {
            glUniform3f(loc, 0.7, 0.14, 0.14);
        }

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
    case 'r':
        initGraph();
        break;
    }
}

std::vector<float> loadModel() {
    std::string inputfile = "vertice.obj";
    tinyobj::ObjReaderConfig reader_config;
    reader_config.mtl_search_path = "./"; // Path to material files

    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(inputfile, reader_config)) {
        if (!reader.Error().empty()) {
            std::cerr << "TinyObjReader: " << reader.Error();
        }
        exit(1);
    }

    if (!reader.Warning().empty()) {
        std::cout << "TinyObjReader: " << reader.Warning();
    }

    auto &attrib = reader.GetAttrib();
    auto &shapes = reader.GetShapes();
    auto &materials = reader.GetMaterials();

    std::vector<float> vertices;

    // Loop over shapes
    for (size_t s = 0; s < shapes.size(); s++) {
        // Loop over faces(polygon)
        size_t index_offset = 0;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
            size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);

            // Loop over vertices in the face.
            for (size_t v = 0; v < fv; v++) {
                // access to vertex
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
                tinyobj::real_t vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                tinyobj::real_t vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                tinyobj::real_t vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

                tinyobj::real_t nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
                tinyobj::real_t ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
                tinyobj::real_t nz = attrib.normals[3 * size_t(idx.normal_index) + 2];

                vertices.push_back(vx);
                vertices.push_back(vy);
                vertices.push_back(vz);
                vertices.push_back(nx);
                vertices.push_back(ny);
                vertices.push_back(nz);
            }
            index_offset += fv;
        }
    }

    return vertices;
}

/**
 * Init vertex data.
 *
 * Defines the coordinates for vertices, creates the arrays for OpenGL.
 */
void initData() {
    // Set cube vertices.
    float cubo[] = {
        // coordinate        // normal
        -0.5f, -0.5f, 0.5f,  0.0f,  0.0f,  1.0f,  //
        0.5f,  -0.5f, 0.5f,  0.0f,  0.0f,  1.0f,  //
        0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  //
        -0.5f, -0.5f, 0.5f,  0.0f,  0.0f,  1.0f,  //
        0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  //
        -0.5f, 0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  //
        0.5f,  -0.5f, 0.5f,  1.0f,  0.0f,  0.0f,  //
        0.5f,  -0.5f, -0.5f, 1.0f,  0.0f,  0.0f,  //
        0.5f,  0.5f,  -0.5f, 1.0f,  0.0f,  0.0f,  //
        0.5f,  -0.5f, 0.5f,  1.0f,  0.0f,  0.0f,  //
        0.5f,  0.5f,  -0.5f, 1.0f,  0.0f,  0.0f,  //
        0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  //
        0.5f,  -0.5f, -0.5f, 0.0f,  0.0f,  -1.0f, //
        -0.5f, -0.5f, -0.5f, 0.0f,  0.0f,  -1.0f, //
        -0.5f, 0.5f,  -0.5f, 0.0f,  0.0f,  -1.0f, //
        0.5f,  -0.5f, -0.5f, 0.0f,  0.0f,  -1.0f, //
        -0.5f, 0.5f,  -0.5f, 0.0f,  0.0f,  -1.0f, //
        0.5f,  0.5f,  -0.5f, 0.0f,  0.0f,  -1.0f, //
        -0.5f, -0.5f, -0.5f, -1.0f, 0.0f,  0.0f,  //
        -0.5f, -0.5f, 0.5f,  -1.0f, 0.0f,  0.0f,  //
        -0.5f, 0.5f,  0.5f,  -1.0f, 0.0f,  0.0f,  //
        -0.5f, -0.5f, -0.5f, -1.0f, 0.0f,  0.0f,  //
        -0.5f, 0.5f,  0.5f,  -1.0f, 0.0f,  0.0f,  //
        -0.5f, 0.5f,  -0.5f, -1.0f, 0.0f,  0.0f,  //
        -0.5f, 0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  //
        0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  //
        0.5f,  0.5f,  -0.5f, 0.0f,  1.0f,  0.0f,  //
        -0.5f, 0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  //
        0.5f,  0.5f,  -0.5f, 0.0f,  1.0f,  0.0f,  //
        -0.5f, 0.5f,  -0.5f, 0.0f,  1.0f,  0.0f,  //
        -0.5f, -0.5f, 0.5f,  0.0f,  -1.0f, 0.0f,  //
        -0.5f, -0.5f, -0.5f, 0.0f,  -1.0f, 0.0f,  //
        0.5f,  -0.5f, 0.5f,  0.0f,  -1.0f, 0.0f,  //
        -0.5f, -0.5f, -0.5f, 0.0f,  -1.0f, 0.0f,  //
        0.5f,  -0.5f, -0.5f, 0.0f,  -1.0f, 0.0f,  //
        0.5f,  -0.5f, 0.5f,  0.0f,  -1.0f, 0.0f,
    };

    std::vector<float> casinha = loadModel();

    // Vertex array.
    glGenVertexArrays(1, &VAO_CUBO);
    glBindVertexArray(VAO_CUBO);

    // Vertex buffer
    glGenBuffers(1, &VBO_CASA);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_CASA);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubo), cubo, GL_STATIC_DRAW);

    // Set attributes.
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Vertex array.
    glGenVertexArrays(1, &VAO_CASA);
    glBindVertexArray(VAO_CASA);

    // Vertex buffer
    glGenBuffers(1, &VBO_CASA);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_CASA);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * casinha.size(), casinha.data(), GL_STATIC_DRAW);

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

/// Reseta o gráfo para o estado inicial.
void initGraph() {
    nodes = std::vector<Node>();
    for (int i = 0; i < 25; i++) {
        int x = i / 5;
        int y = i % 5;
        float dx = 1.0 * ((float)rand() / (float)(RAND_MAX)-1.0);
        float dy = 1.0 * ((float)rand() / (float)(RAND_MAX)-1.0);

        auto position = glm::vec3((float)x * 2.0f - 4.0f + dx, 0.0f, -4.0f + 2.0f * (float)y + dy);
        nodes.push_back(
            Node{.position = position, .in_tree = false, .connected_to = -1, .cost = 1.0f / 0.0f});
    }
    not_included.clear();
    for (int v = 0; v < nodes.size(); v++) {
        not_included.push_back(v);
    }
    for (int i = 0; i < nodes.size() - 1; i++) {
        int r = i + (rand() % (not_included.size() - i));
        std::swap(not_included[i], not_included[r]);
    }
}

/// Roda uma iteração do algoritmo Prim.
///
/// beseado em: https://en.wikipedia.org/wiki/Prim%27s_algorithm#Description
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
