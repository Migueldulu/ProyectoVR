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

#include "stb_truetype.h"

class PosicionesApp : public OVRFW::XrApp {
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

    // rendering fuente
    GLuint fontTexture;
    stbtt_packedchar fontGlyphs[96]; // ASCII 32-127
    int fontAtlasWidth = 512;
    int fontAtlasHeight = 512;
    float fontPixelHeight = 48.0f;

    // Variables para texto dinámico
    std::string currentTimeText;
    float animationTime = 0.0f;

public:
    PosicionesApp() : OVRFW::XrApp() {
        BackgroundColor = OVR::Vector4f(0.1f, 0.1f, 0.1f, 1.0f);
        framebuffer = shaderProgram = VAO = VBO = 0;
        std::memset(&textOverlayLayer, 0, sizeof(textOverlayLayer));
        textOverlayLayer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
    }

    virtual std::vector<const char*> GetExtensions() override {
        std::vector<const char*> extensions = XrApp::GetExtensions();
        return extensions;
    }

    //Aqui se crea la sesion OpenXR
    virtual bool AppInit(const xrJava* context) override {
        ALOG("Posiciones AppInit iniciado");
        return true;
    }

    //Se puede crear recursos OpenXR porque ahora SI existe la sesion
    virtual bool SessionInit() override {
        ALOG("Posiciones SessionInit iniciado");

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

        if (!CargarFontAtlas("res/font/robotocondensed_regular.ttf")){
            ALOG("ERROR: ha fallado cargar el atlas");
            return false;
        }

        // Configurar la compositor layer
        SetupCompositorLayer();

        ALOG("Posiciones SessionInit completado exitosamente");
        return true;
    }

    virtual void Update(const OVRFW::ovrApplFrameIn& in) override {
        // Actualizar el tiempo de animación
        animationTime += in.DeltaSeconds;

        // Actualizar el texto cada frame
        UpdateTimeText();

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
        ALOG("Posiciones SessionEnd completado");
    }

    virtual void AppShutdown(const xrJava* context) override {
        OVRFW::XrApp::AppShutdown(context);
        ALOG("Posiciones AppShutdown completado");
    }

private:
    void UpdateTimeText() {
        // Obtener tiempo actual
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;

        std::stringstream ss;
        ss << "Hora actual Spain: ";
        ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
        ss << ":" << std::setfill('0') << std::setw(3) << ms.count();

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

        glUniform3f(glGetUniformLocation(shaderProgram, "textColor"), 0.0f, 1.0f, 0.0f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fontTexture);

        float x = -0.95f * OVERLAY_WIDTH / 2.0f;
        float y = 0.0f;

        for (char c : currentTimeText) {
            if (c < 32 || c >= 128) continue;
            stbtt_aligned_quad q;
            stbtt_GetPackedQuad(fontGlyphs, fontAtlasWidth, fontAtlasHeight, c - 32, &x, &y, &q, 1);

            float w = OVERLAY_WIDTH;
            float h = OVERLAY_HEIGHT;
            float vertices[6][4] = {
                    { q.x0 / w * 2 - 1, q.y0 / h * 2 - 1, q.s0, q.t0 },
                    { q.x1 / w * 2 - 1, q.y0 / h * 2 - 1, q.s1, q.t0 },
                    { q.x1 / w * 2 - 1, q.y1 / h * 2 - 1, q.s1, q.t1 },
                    { q.x0 / w * 2 - 1, q.y0 / h * 2 - 1, q.s0, q.t0 },
                    { q.x1 / w * 2 - 1, q.y1 / h * 2 - 1, q.s1, q.t1 },
                    { q.x0 / w * 2 - 1, q.y1 / h * 2 - 1, q.s0, q.t1 }
            };
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

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
            uniform vec2 offset; // desplazamiento global del texto
            void main() {
                gl_Position = vec4(aPos + offset, 0.0, 1.0);
                TexCoord = aTexCoord;
            }
        )";

        const char* fragmentShaderSource = R"(
            #version 300 es
            precision mediump float;
            in vec2 TexCoord;
            out vec4 FragColor;
            uniform sampler2D fontAtlas;
            uniform vec3 textColor;
            void main() {
                float alpha = texture(fontAtlas, TexCoord).r;
                FragColor = vec4(textColor, alpha);
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

        // Configurar geometría
        float vertices[] = {
                0,0, 0,0,  1,0, 1,0,  1,1, 1,1
        };

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);

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
        if (fontTexture != 0){
            glDeleteTextures(1, &fontTexture);
            fontTexture = 0;
        }
    }

    bool CargarFontAtlas(const char* pathFuente) {
        FILE* fontFile = fopen(pathFuente, "rb");
        if (!fontFile) {
            ALOG("Error: No se pudo abrir la fuente %s", pathFuente);
            return false;
        }
        fseek(fontFile, 0, SEEK_END);
        size_t size = ftell(fontFile);
        fseek(fontFile, 0, SEEK_SET);

        unsigned char* fontBuffer = new unsigned char[size];
        fread(fontBuffer, 1, size, fontFile);
        fclose(fontFile);

        unsigned char* atlasBitmap = new unsigned char[fontAtlasWidth * fontAtlasHeight];
        memset(atlasBitmap, 0, fontAtlasWidth * fontAtlasHeight);

        stbtt_pack_context pc;
        stbtt_PackBegin(&pc, atlasBitmap, fontAtlasWidth, fontAtlasHeight, 0, 1, nullptr);
        stbtt_PackSetOversampling(&pc, 2, 2); // Suavizado mejorado
        stbtt_PackFontRange(&pc, fontBuffer, 0, fontPixelHeight, 32, 96, fontGlyphs);
        stbtt_PackEnd(&pc);

        // Subir a OpenGL
        glGenTextures(1, &fontTexture);
        glBindTexture(GL_TEXTURE_2D, fontTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, fontAtlasWidth, fontAtlasHeight, 0, GL_RED, GL_UNSIGNED_BYTE, atlasBitmap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        delete[] atlasBitmap;
        delete[] fontBuffer;

        return true;
    }

};

// Punto de entrada
ENTRY_POINT(PosicionesApp)