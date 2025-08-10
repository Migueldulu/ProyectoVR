// main.cpp (versión corregida)
#include <vector>
#include <string>
#include <memory>
#include <cstring>
#include <iostream>

#include <openxr/openxr.h>
#include <GLES3/gl3.h>

#include "XrApp.h"
#include "OVR_Math.h"
#include "Misc/Log.h"


class SegundoPlanoApp : public OVRFW::XrApp {
private:
    // Compositor Layer para el overlay de texto
    XrCompositionLayerQuad textOverlayLayer;
    XrSwapchain textSwapchain;

    // Configuración del overlay
    bool overlayEnabled = true;
    static const int OVERLAY_WIDTH = 512;
    static const int OVERLAY_HEIGHT = 128;

    // OpenGL resources
    GLuint framebuffer;
    GLuint shaderProgram;
    GLuint VAO, VBO, EBO;

public:
    SegundoPlanoApp() : OVRFW::XrApp() {
        BackgroundColor = OVR::Vector4f(0.1f, 0.1f, 0.1f, 1.0f);
        textSwapchain = XR_NULL_HANDLE;
        framebuffer = 0;
        shaderProgram = 0;
        VAO = VBO = EBO = 0;

        // Inicializar la layer de forma segura
        std::memset(&textOverlayLayer, 0, sizeof(textOverlayLayer));
        textOverlayLayer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
    }

    virtual std::vector<const char*> GetExtensions() override {
        std::vector<const char*> extensions = XrApp::GetExtensions();
        return extensions;
    }

    virtual bool AppInit(const xrJava* context) override {
        ALOG("SegundoPlano AppInit iniciado");
        return true;
    }

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

    // Implementar PostProjectionAddLayer para añadir nuestro overlay
    virtual void PostProjectionAddLayer(OVRFW::XrApp::xrCompositorLayerUnion* layers, int& layerCount) override {
        if (overlayEnabled && textSwapchain != XR_NULL_HANDLE) {
            // Asegurarse de añadir la dirección del struct (se copia la estructura)
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
    bool CreateTextOverlaySwapchain() {
        // Configurar el formato del swapchain
        XrSwapchainCreateInfo swapchainCreateInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        swapchainCreateInfo.arraySize = 1;
        // OpenXR espera int64_t para format; casteamos desde define GL_RGBA8
        swapchainCreateInfo.format = static_cast<int64_t>(GL_RGBA8);
        swapchainCreateInfo.width = OVERLAY_WIDTH;
        swapchainCreateInfo.height = OVERLAY_HEIGHT;
        swapchainCreateInfo.mipCount = 1;
        swapchainCreateInfo.faceCount = 1;
        swapchainCreateInfo.sampleCount = 1;
        swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;

        XrResult result = xrCreateSwapchain(Session, &swapchainCreateInfo, &textSwapchain);
        if (XR_FAILED(result)) {
            ALOG("ERROR: xrCreateSwapchain falló: %d", result);
            return false;
        }

        ALOG("Swapchain creado exitosamente: %dx%d", OVERLAY_WIDTH, OVERLAY_HEIGHT);
        return true;
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

        // Shader fragment para texto
        const char* fragmentShaderSource = R"(
            #version 300 es
            precision mediump float;
            out vec4 FragColor;
            in vec2 TexCoord;
            uniform vec3 textColor;
            void main() {
                // Por ahora, color sólido en el área del texto
                if (TexCoord.x > 0.1 && TexCoord.x < 0.9 &&
                    TexCoord.y > 0.3 && TexCoord.y < 0.7) {
                    FragColor = vec4(textColor, 1.0);
                } else {
                    FragColor = vec4(0.0, 0.0, 0.0, 0.0); // Transparente
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

        // Crear programa shader
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
        // Aseguramos que .type ya está seteado en constructor
        // textOverlayLayer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;

        // Configurar flags para blend y transparencia (si el runtime soporta)
        textOverlayLayer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;

        // Posición del overlay: fija relativa al headset
        // Nota: "HeadSpace" debe existir en tu XrApp base. Si no, reemplazar por el reference space correcto.
        textOverlayLayer.space = HeadSpace;

        // Inicializar pose explícitamente
        textOverlayLayer.pose.orientation.x = 0.0f;
        textOverlayLayer.pose.orientation.y = 0.0f;
        textOverlayLayer.pose.orientation.z = 0.0f;
        textOverlayLayer.pose.orientation.w = 1.0f;

        textOverlayLayer.pose.position.x = 0.3f;
        textOverlayLayer.pose.position.y = 0.2f;
        textOverlayLayer.pose.position.z = -0.5f;

        // Tamaño del quad (en metros)
        textOverlayLayer.size = {0.3f, 0.08f}; // 30cm x 8cm

        // Configurar subimagen del swapchain
        textOverlayLayer.subImage.swapchain = textSwapchain;
        textOverlayLayer.subImage.imageRect.offset = {0, 0};
        textOverlayLayer.subImage.imageRect.extent = {OVERLAY_WIDTH, OVERLAY_HEIGHT};
        textOverlayLayer.subImage.imageArrayIndex = 0;

        ALOG("Compositor layer configurada en posición (%.2f, %.2f, %.2f)",
             textOverlayLayer.pose.position.x,
             textOverlayLayer.pose.position.y,
             textOverlayLayer.pose.position.z);
    }

    void UpdateTextOverlay() {
        // Obtener la imagen actual del swapchain
        uint32_t imageIndex = 0;
        XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        XrResult res = xrAcquireSwapchainImage(textSwapchain, &acquireInfo, &imageIndex);
        if (XR_FAILED(res)) {
            ALOG("ERROR: xrAcquireSwapchainImage falló: %d", res);
            return;
        }

        XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        waitInfo.timeout = XR_INFINITE_DURATION;
        res = xrWaitSwapchainImage(textSwapchain, &waitInfo);
        if (XR_FAILED(res)) {
            ALOG("ERROR: xrWaitSwapchainImage falló: %d", res);
            return;
        }

        // Obtener las imágenes del swapchain
        uint32_t imageCount = 0;
        xrEnumerateSwapchainImages(textSwapchain, 0, &imageCount, nullptr);
        std::vector<XrSwapchainImageOpenGLESKHR> swapchainImages;
        swapchainImages.resize(imageCount);
        for (uint32_t i = 0; i < imageCount; ++i) {
            swapchainImages[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
            swapchainImages[i].next = nullptr;
        }
        res = xrEnumerateSwapchainImages(textSwapchain, imageCount, &imageCount,
                                         reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchainImages.data()));
        if (XR_FAILED(res)) {
            ALOG("ERROR: xrEnumerateSwapchainImages falló: %d", res);
            // intentar liberar la imagen antes de salir
            XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            xrReleaseSwapchainImage(textSwapchain, &releaseInfo);
            return;
        }

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

        // Habilitar blending para transparencia
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Usar nuestro shader
        glUseProgram(shaderProgram);

        // Configurar color del texto (verde brillante)
        GLint colorLocation = glGetUniformLocation(shaderProgram, "textColor");
        if (colorLocation >= 0) {
            glUniform3f(colorLocation, 0.0f, 1.0f, 0.0f);
        }

        // Renderizar el quad
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        glDisable(GL_BLEND);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
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

// Punto de entrada de la aplicación usando la macro del framework
ENTRY_POINT(SegundoPlanoApp)
