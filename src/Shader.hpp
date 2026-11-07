#pragma once

#include <glad/glad.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

class Shader {
  public:
    GLuint ID = 0;

    Shader(const char* vertexPath, const char* fragmentPath) {
        std::string vertexCode;
        std::string fragmentCode;

        std::ifstream vertexFile(vertexPath);
        std::ifstream fragmentFile(fragmentPath);

        if (!vertexFile || !fragmentFile) {
            std::cerr << "Failed to open shader files: " << vertexPath << " or " << fragmentPath << '\n';
            return;
        }

        std::stringstream vertexStream;
        std::stringstream fragmentStream;

        vertexStream << vertexFile.rdbuf();
        fragmentStream << fragmentFile.rdbuf();

        vertexCode = vertexStream.str();
        fragmentCode = fragmentStream.str();

        GLuint vertex = compile(GL_VERTEX_SHADER, vertexCode.c_str());
        GLuint fragment = compile(GL_FRAGMENT_SHADER, fragmentCode.c_str());

        if (!vertex || !fragment) {
            if (vertex) glDeleteShader(vertex);
            if (fragment) glDeleteShader(fragment);
            return;
        }

        ID = glCreateProgram();

        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        glLinkProgram(ID);

        GLint success;
        glGetProgramiv(ID, GL_LINK_STATUS, &success);

        if (!success) {
            char log[512];
            glGetProgramInfoLog(ID, 512, nullptr, log);
            std::cerr << "Shader linking failed:\n" << log << '\n';

            glDeleteProgram(ID);
            ID = 0;
        }

        glDeleteShader(vertex);
        glDeleteShader(fragment);
    }

    ~Shader() {
        if (ID)
            glDeleteProgram(ID);
    }

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void use() const {
        glUseProgram(ID);
    }

    void setInt(const std::string& name, int value) const {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
    }

    void setFloat(const std::string& name, float value) const {
        glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
    }

    void setVec2(const std::string& name, float x, float y) const {
        glUniform2f(glGetUniformLocation(ID, name.c_str()), x, y);
    }

    void setVec3(const std::string& name, float x, float y, float z) const {
        glUniform3f(glGetUniformLocation(ID, name.c_str()), x, y, z);
    }

  private:
    GLuint compile(GLenum type, const char* source) {
        GLuint shader = glCreateShader(type);

        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

        if (!success) {
            char log[512];
            glGetShaderInfoLog(shader, 512, nullptr, log);
            std::cerr << "Shader compilation failed:\n" << log << '\n';

            glDeleteShader(shader);
            return 0;
        }

        return shader;
    }
};

