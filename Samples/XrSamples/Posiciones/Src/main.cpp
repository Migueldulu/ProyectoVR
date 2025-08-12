#include <vector>
#include <string>
#include <memory>
#include <cstring>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>

#include <openxr/openxr.h>
#include <GLES3/gl3.h>

#include "XrApp.h"
#include "OVR_Math.h"
#include "Misc/Log.h"

class SegundoPlanoApp : public OVRFW::XrApp {
private:
    // Compositor Layer para el overlay de texto (Quad=Rectangulo)
    XrCompositionLayerQuad textOverlayLayer;
    XrSwapchain textSwapchain;

    // Configuración del overlay
    bool overlayEnabled = true;
    static const int OVERLAY_WIDTH = 1024;
    static const int OVERLAY_HEIGHT = 256;

    // OpenGL resources
    GLuint framebuffer;
    GLuint shaderProgram;
    GLuint VAO, VBO, EBO;

    // Variables para texto dinámico
    std::string currentTimeText;
    std::chrono::steady_clock::time_point lastTimeUpdate;
    float animationTime = 0.0f;

public:
    SegundoPlanoApp() : OVRFW::XrApp() {
        BackgroundColor = OVR::Vector4f(0.1f, 0.1f, 0.1f, 1.0f);
        textSwapchain = XR_NULL_HANDLE; // El valor significa no-inicializado
        framebuffer = 0;
        shaderProgram = 0;
        VAO = VBO = EBO = 0;

        // Memory set para inicializacion de forma segura
        std::memset(&textOverlayLayer, 0, sizeof(textOverlayLayer));
        textOverlayLayer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;

        lastTimeUpdate = std::chrono::steady_clock::now();
        UpdateTimeText();
    }

    virtual std::vector<const char*> GetExtensions() override {
        std::vector<const char*> extensions = XrApp::GetExtensions();
        return extensions;
    }

    //Aqui se crea la sesion OpenXR
    virtual bool AppInit(const xrJava* context) override {
        ALOG("SegundoPlano AppInit iniciado");
        return true;
    }

    //Se puede crear recursos OpenXR porque ahora SI existe la sesion
    virtual bool SessionInit() override {
        ALOG("SegundoPlano SessionInit iniciado");

        // Crear swapchain para el overlay de texto
        if (!CreateTextOverlaySwapchain()) {
            ALOG("ERROR: No se pudo crear el swapchain para el overlay");
            return false;
        }

        // Configurar OpenGL para renderizar texto
        if (!SetupTextRendering()) {
            ALOG("ERROR: No se pudo configurar el renderizado de texto");
            return false;
        }

        // Configurar la compositor layer
        SetupCompositorLayer();

        ALOG("SegundoPlano SessionInit completado exitosamente");
        return true;
    }

    virtual void Update(const OVRFW::ovrApplFrameIn& in) override {
        // Actualizar el tiempo de animación
        animationTime += in.DeltaSeconds;

        // Actualizar el texto cada segundo
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTimeUpdate);
        if (duration.count() >= 1000) { // Actualizar cada 1000ms (1 segundo)
            UpdateTimeText();
            lastTimeUpdate = now;
        }

        // Actualizar el contenido del overlay cada frame
        if (overlayEnabled && textSwapchain != XR_NULL_HANDLE) {
            UpdateTextOverlay();
        }

        // Toggle overlay con botón A
        if (in.Clicked(OVRFW::ovrApplFrameIn::kButtonA)) {
            overlayEnabled = !overlayEnabled;
            ALOG("Overlay %s", overlayEnabled ? "activado" : "desactivado");
        }
    }

    virtual void Render(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out) override {
        // La aplicación base puede renderizar su contenido aquí
    }

    // Se ejecuta en el main loop despues de renderizar el contenido  (si quisieramos capas detras del contenido se usaria PreProjectionAddLayer)
    virtual void PostProjectionAddLayer(OVRFW::XrApp::xrCompositorLayerUnion* layers, int& layerCount) override {
        if (overlayEnabled && textSwapchain != XR_NULL_HANDLE) {
            // Accedemos al tipo quad en la union que contiene los tipos de layer
            layers[layerCount++].Quad = textOverlayLayer;
            ALOG("Compositor layer añadida, total layers: %d", layerCount);
        }
    }

    virtual void SessionEnd() override {
        CleanupTextRendering();
        if (textSwapchain != XR_NULL_HANDLE) {
            xrDestroySwapchain(textSwapchain);
            textSwapchain = XR_NULL_HANDLE;
        }
        ALOG("SegundoPlano SessionEnd completado");
    }

    virtual void AppShutdown(const xrJava* context) override {
        OVRFW::XrApp::AppShutdown(context);
        ALOG("SegundoPlano AppShutdown completado");
    }

private:
    void UpdateTimeText() {
        // Obtener tiempo actual
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;

        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
        ss << "." << std::setfill('0') << std::setw(3) << ms.count();

        currentTimeText = ss.str();
        ALOG("Tiempo actualizado: %s", currentTimeText.c_str());
    }

    bool CreateTextOverlaySwapchain() {
        // Configurar el formato del swapchain
        XrSwapchainCreateInfo swapchainCreateInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        swapchainCreateInfo.arraySize = 1; // 1 para Imagen 2D normal
        swapchainCreateInfo.format = static_cast<int64_t>(GL_RGBA8); // OpenXR espera int64_t para format
        swapchainCreateInfo.width = OVERLAY_WIDTH;
        swapchainCreateInfo.height = OVERLAY_HEIGHT;
        swapchainCreateInfo.mipCount = 1; //sin mipmaps
        swapchainCreateInfo.faceCount = 1; //sin cubemap
        swapchainCreateInfo.sampleCount = 1; //sin multisampling
        swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;

        XrResult result = xrCreateSwapchain(Session, &swapchainCreateInfo, &textSwapchain);
        if (XR_FAILED(result)) {
            ALOG("ERROR: xrCreateSwapchain falló: %d", result);
            return false;
        }

        ALOG("Swapchain creado exitosamente: %dx%d", OVERLAY_WIDTH, OVERLAY_HEIGHT);
        return true;
    }

    void UpdateTextOverlay() {
        // Obtener imagen actual del swapchain
        XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        uint32_t imageIndex;
        XrResult result = xrAcquireSwapchainImage(textSwapchain, &acquireInfo, &imageIndex);
        if (XR_FAILED(result)) {
            ALOG("ERROR: xrAcquireSwapchainImage falló: %d", result);
            return;
        }

        // Esperar a que la imagen esté disponible
        XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        waitInfo.timeout = XR_INFINITE_DURATION;
        result = xrWaitSwapchainImage(textSwapchain, &waitInfo);
        if (XR_FAILED(result)) {
            ALOG("ERROR: xrWaitSwapchainImage falló: %d", result);
            XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            xrReleaseSwapchainImage(textSwapchain, &releaseInfo);
            return;
        }

        // Obtener las imágenes del swapchain
        uint32_t imageCount;
        xrEnumerateSwapchainImages(textSwapchain, 0, &imageCount, nullptr);
        std::vector<XrSwapchainImageOpenGLESKHR> swapchainImages(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});
        xrEnumerateSwapchainImages(textSwapchain, imageCount, &imageCount,
                                   reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchainImages.data()));

        if (imageIndex >= imageCount) {
            ALOG("ERROR: imageIndex fuera de rango: %u >= %u", imageIndex, imageCount);
            XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            xrReleaseSwapchainImage(textSwapchain, &releaseInfo);
            return;
        }

        // Renderizar el contenido del texto
        RenderTextToTexture(swapchainImages[imageIndex].image);

        // Liberar la imagen
        XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        xrReleaseSwapchainImage(textSwapchain, &releaseInfo);
    }

    void RenderTextToTexture(GLuint texture) {
        // Crear framebuffer si no existe
        if (framebuffer == 0) {
            glGenFramebuffers(1, &framebuffer);
        }

        // Configurar framebuffer para renderizar a la textura
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            ALOG("ERROR: Framebuffer incompleto: 0x%x", status);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return;
        }

        // Configurar viewport
        glViewport(0, 0, OVERLAY_WIDTH, OVERLAY_HEIGHT);

        // Limpiar con transparencia
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Activa el mezclado alfa
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Usar nuestro shader
        glUseProgram(shaderProgram);

        // Configurar color del texto (verde brillante con pulsación)
        float pulse = 0.8f + 0.2f * sinf(animationTime * 2.0f);
        GLint colorLocation = glGetUniformLocation(shaderProgram, "textColor");
        if (colorLocation >= 0) {
            glUniform3f(colorLocation, 0.0f, pulse, 0.2f);
        }

        // Pasar el tiempo para animación
        GLint timeLocation = glGetUniformLocation(shaderProgram, "time");
        if (timeLocation >= 0) {
            glUniform1f(timeLocation, animationTime);
        }

        // Pasar la cadena de tiempo como uniforms (simplificado para los primeros 8 caracteres)
        GLint digitsLocation = glGetUniformLocation(shaderProgram, "timeDigits");
        if (digitsLocation >= 0) {
            float digits[12] = {0.0f}; // Máximo 12 caracteres
            for (int i = 0; i < std::min(12, (int)currentTimeText.length()); ++i) {
                char c = currentTimeText[i];
                if (c >= '0' && c <= '9') {
                    digits[i] = float(c - '0'); // 0-9
                } else if (c == ':') {
                    digits[i] = 10.0f; // Carácter especial para :
                } else if (c == '.') {
                    digits[i] = 11.0f; // Carácter especial para .
                } else {
                    digits[i] = -1.0f; // Carácter no reconocido
                }
            }
            glUniform1fv(digitsLocation, 12, digits);
        }

        // Renderizar el quad
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        glDisable(GL_BLEND);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    bool SetupTextRendering() {
        // Shader vertex simple
        const char* vertexShaderSource = R"(
            #version 300 es
            precision mediump float;
            layout (location = 0) in vec2 aPos;
            layout (location = 1) in vec2 aTexCoord;
            out vec2 TexCoord;
            void main() {
                gl_Position = vec4(aPos, 0.0, 1.0);
                TexCoord = aTexCoord;
            }
        )";

        // Shader fragment avanzado para renderizar texto dinámico usando patrones de bits
        const char* fragmentShaderSource = R"(
            #version 300 es
            precision mediump float;
            out vec4 FragColor;
            in vec2 TexCoord;
            uniform vec3 textColor;
            uniform float time;
            uniform float timeDigits[12];

            // Patrones de dígitos en formato bitmap 5x7
            const int digitPatterns[12 * 7] = int[](
                // Dígito 0
                14, 17, 17, 17, 17, 17, 14,
                // Dígito 1
                4, 12, 4, 4, 4, 4, 14,
                // Dígito 2
                14, 17, 1, 14, 16, 16, 31,
                // Dígito 3
                14, 17, 1, 6, 1, 17, 14,
                // Dígito 4
                17, 17, 17, 31, 1, 1, 1,
                // Dígito 5
                31, 16, 16, 30, 1, 1, 30,
                // Dígito 6
                14, 16, 16, 30, 17, 17, 14,
                // Dígito 7
                31, 1, 2, 4, 8, 8, 8,
                // Dígito 8
                14, 17, 17, 14, 17, 17, 14,
                // Dígito 9
                14, 17, 17, 15, 1, 1, 14,
                // Carácter : (dos puntos)
                0, 4, 4, 0, 4, 4, 0,
                // Carácter . (punto)
                0, 0, 0, 0, 0, 4, 4
            );

            bool getPixel(int digit, int x, int y) {
                if (digit < 0 || digit >= 12 || x < 0 || x >= 5 || y < 0 || y >= 7) {
                    return false;
                }
                int patternIndex = digit * 7 + y;
                int pattern = digitPatterns[patternIndex];
                return ((pattern >> (4 - x)) & 1) == 1;
            }

            void main() {
                vec2 uv = TexCoord;

                // Área de renderizado del texto
                float textAreaLeft = 0.05;
                float textAreaRight = 0.95;
                float textAreaTop = 0.35;
                float textAreaBottom = 0.65;

                if (uv.x < textAreaLeft || uv.x > textAreaRight ||
                    uv.y < textAreaTop || uv.y > textAreaBottom) {
                    FragColor = vec4(0.0, 0.0, 0.0, 0.0);
                    return;
                }

                // Normalizar coordenadas dentro del área de texto
                float textU = (uv.x - textAreaLeft) / (textAreaRight - textAreaLeft);
                float textV = (uv.y - textAreaTop) / (textAreaBottom - textAreaTop);

                // Calcular qué carácter estamos renderizando
                float charWidth = 1.0 / 12.0; // Espacio para 12 caracteres
                int charIndex = int(textU / charWidth);

                if (charIndex >= 12) {
                    FragColor = vec4(0.0, 0.0, 0.0, 0.0);
                    return;
                }

                // Coordenadas dentro del carácter actual
                float charU = mod(textU, charWidth) / charWidth;
                float charV = textV;

                // Convertir a coordenadas de pixel en el bitmap 5x7
                int pixelX = int(charU * 5.0);
                int pixelY = int((1.0 - charV) * 7.0); // Invertir Y

                // Obtener el dígito para este carácter
                int digit = int(timeDigits[charIndex]);

                // Verificar si este pixel debe estar encendido
                bool pixelOn = getPixel(digit, pixelX, pixelY);

                if (pixelOn) {
                    // Agregar efecto de brillo pulsante
                    float glow = 1.0 + 0.3 * sin(time * 3.0 + float(charIndex) * 0.5);
                    FragColor = vec4(textColor * glow, 1.0);
                } else {
                    FragColor = vec4(0.0, 0.0, 0.0, 0.0);
                }
            }
        )";

        // Compilar shaders
        GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vertexShaderSource);
        GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

        if (vertexShader == 0 || fragmentShader == 0) {
            if (vertexShader) glDeleteShader(vertexShader);
            if (fragmentShader) glDeleteShader(fragmentShader);
            return false;
        }

        // Crear programa de shader
        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);

        // Verificar linkeo
        GLint success = 0;
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
        if (!success) {
            GLint logLength = 0;
            glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &logLength);
            std::string infoLog;
            infoLog.resize(logLength > 0 ? logLength : 1);
            glGetProgramInfoLog(shaderProgram, logLength, nullptr, &infoLog[0]);
            ALOG("ERROR: Linkeo del shader falló: %s", infoLog.c_str());
            glDeleteProgram(shaderProgram);
            shaderProgram = 0;
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            return false;
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        // Configurar geometría (quad completo)
        float vertices[] = {
                // positions    // texture coords
                -1.0f, -1.0f,   0.0f, 0.0f,
                1.0f, -1.0f,   1.0f, 0.0f,
                1.0f,  1.0f,   1.0f, 1.0f,
                -1.0f,  1.0f,   0.0f, 1.0f
        };

        GLuint indices[] = {
                0, 1, 2,
                2, 3, 0
        };

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);

        ALOG("Configuración de renderizado de texto completada");
        return true;
    }

    GLuint CompileShader(GLenum type, const char* source) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        GLint success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            GLint logLength = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
            std::string infoLog;
            infoLog.resize(logLength > 0 ? logLength : 1);
            glGetShaderInfoLog(shader, logLength, nullptr, &infoLog[0]);
            ALOG("ERROR: Compilación del shader falló: %s", infoLog.c_str());
            glDeleteShader(shader);
            return 0;
        }

        return shader;
    }

    void SetupCompositorLayer() {
        // Configurar la compositor layer
        textOverlayLayer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
        textOverlayLayer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
        textOverlayLayer.space = HeadSpace;
        textOverlayLayer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;

        // Posición: delante del usuario, ligeramente hacia arriba
        textOverlayLayer.pose.position.x = 0.4f;    // derecha
        textOverlayLayer.pose.position.y = -0.3f;  // abajo
        textOverlayLayer.pose.position.z = -1.5f; // Distancia cómoda
        textOverlayLayer.pose.orientation.x = 0.0f;
        textOverlayLayer.pose.orientation.y = 0.0f;
        textOverlayLayer.pose.orientation.z = 0.0f;
        textOverlayLayer.pose.orientation.w = 1.0f;

        // Tamaño del quad (más grande para texto)
        textOverlayLayer.size.width = 0.3f;
        textOverlayLayer.size.height = 0.08f;

        // Configurar el submuestreo (imagen completa)
        textOverlayLayer.subImage.swapchain = textSwapchain;
        textOverlayLayer.subImage.imageRect.offset.x = 0;
        textOverlayLayer.subImage.imageRect.offset.y = 0;
        textOverlayLayer.subImage.imageRect.extent.width = OVERLAY_WIDTH;
        textOverlayLayer.subImage.imageRect.extent.height = OVERLAY_HEIGHT;
        textOverlayLayer.subImage.imageArrayIndex = 0;

        ALOG("Compositor layer configurada: posición (%.2f, %.2f, %.2f), tamaño (%.2f, %.2f)",
             textOverlayLayer.pose.position.x, textOverlayLayer.pose.position.y, textOverlayLayer.pose.position.z,
             textOverlayLayer.size.width, textOverlayLayer.size.height);
    }

    void CleanupTextRendering() {
        if (framebuffer != 0) {
            glDeleteFramebuffers(1, &framebuffer);
            framebuffer = 0;
        }
        if (VAO != 0) {
            glDeleteVertexArrays(1, &VAO);
            VAO = 0;
        }
        if (VBO != 0) {
            glDeleteBuffers(1, &VBO);
            VBO = 0;
        }
        if (EBO != 0) {
            glDeleteBuffers(1, &EBO);
            EBO = 0;
        }
        if (shaderProgram != 0) {
            glDeleteProgram(shaderProgram);
            shaderProgram = 0;
        }
    }
};

// Punto de entrada
ENTRY_POINT(SegundoPlanoApp)