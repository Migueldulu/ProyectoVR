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
#include "GUI/GuiSys.h"
#include "Render/SurfaceRender.h"

class PosicionesApp : public OVRFW::XrApp {
private:
    XrCompositionLayerQuad textOverlayLayer{};
    XrSwapchain textSwapchain = XR_NULL_HANDLE;

    std::vector<XrSwapchainImageOpenGLESKHR> textSwapchainImages;

    bool overlayEnabled = true;
    static const int OVERLAY_WIDTH = 1024;
    static const int OVERLAY_HEIGHT = 256;

    GLuint framebuffer = 0;

    std::unique_ptr<OVRFW::OvrGuiSys> GuiSys;

    std::string currentTimeText;
    float animationTime = 0.0f;

public:
    PosicionesApp() : OVRFW::XrApp() {
        BackgroundColor = OVR::Vector4f(0.3f, 0.3f, 0.3f, 1.0f);
        std::memset(&textOverlayLayer, 0, sizeof(textOverlayLayer));
        textOverlayLayer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
    }

    virtual std::vector<const char*> GetExtensions() override {
        std::vector<const char*> extensions = XrApp::GetExtensions();
        return extensions;
    }

    virtual bool AppInit(const xrJava* context) override {
        ALOG("main\noverlay: AppInit");
        return true;
    }

    virtual bool SessionInit() override {
        ALOG("main\noverlay: SessionInit start");

        if (!InitializeGuiSystem()) {
            ALOG("main\noverlay: GuiSys Init FAILED");
            return false;
        }
        ALOG("main\noverlay: GuiSys Init done");

        if (!CreateTextOverlaySwapchain()) {
            return false;
        }

        // Cachear imágenes del swapchain
        {
            uint32_t imageCount = 0;
            xrEnumerateSwapchainImages(textSwapchain, 0, &imageCount, nullptr);
            textSwapchainImages.resize(imageCount);
            for (auto& img : textSwapchainImages) { img.type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR; }
            xrEnumerateSwapchainImages(
                    textSwapchain,
                    imageCount,
                    &imageCount,
                    reinterpret_cast<XrSwapchainImageBaseHeader*>(textSwapchainImages.data()));
            ALOG("main\noverlay: swapchain images=%u", imageCount);
        }

        SetupCompositorLayer();

        // Habilita logs internos de SurfaceRender (no necesitas 'extern')
        OVRFW::LogRenderSurfaces = true; // hará spew desde SurfaceRender

        ALOG("main\noverlay: SessionInit ok");
        return true;
    }

    virtual void Update(const OVRFW::ovrApplFrameIn& in) override {
        animationTime += in.DeltaSeconds;
        UpdateTimeText();

        if (GuiSys) {
            GuiSys->Frame(in, OVR::Matrix4f::Identity());
        }

        if (overlayEnabled && textSwapchain != XR_NULL_HANDLE) {
            UpdateTextOverlay();
        }

        if (in.Clicked(OVRFW::ovrApplFrameIn::kButtonA)) {
            overlayEnabled = !overlayEnabled;
            ALOG("main\noverlay: overlay %s", overlayEnabled ? "ON" : "OFF");
        }
    }

    virtual void Render(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out) override { }

    virtual void PostProjectionAddLayer(OVRFW::XrApp::xrCompositorLayerUnion* layers, int& layerCount) override {
        if (overlayEnabled && textSwapchain != XR_NULL_HANDLE) {
            textOverlayLayer.subImage.swapchain = textSwapchain;
            textOverlayLayer.subImage.imageArrayIndex = 0;
            textOverlayLayer.subImage.imageRect.offset = {0, 0};
            textOverlayLayer.subImage.imageRect.extent = { OVERLAY_WIDTH, OVERLAY_HEIGHT };
            layers[layerCount++].Quad = textOverlayLayer;
        }
    }

    virtual void SessionEnd() override {
        CleanupSystems();
        if (textSwapchain != XR_NULL_HANDLE) {
            xrDestroySwapchain(textSwapchain);
            textSwapchain = XR_NULL_HANDLE;
        }
        textSwapchainImages.clear();
        ALOG("main\noverlay: SessionEnd done");
    }

    virtual void AppShutdown(const xrJava* context) override {
        OVRFW::XrApp::AppShutdown(context);
        ALOG("main\noverlay: AppShutdown done");
    }

private:
    void UpdateTimeText() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::stringstream ss;
        ss << "Hora actual Spain: ";
        ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
        ss << ":" << std::setfill('0') << std::setw(3) << ms.count();

        currentTimeText = ss.str();
        ALOG("overlay: timeStr='%s' len=%d", currentTimeText.c_str(), (int)currentTimeText.size());
    }

    bool CreateTextOverlaySwapchain() {
        XrSwapchainCreateInfo sci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        sci.arraySize = 1;
        sci.format = static_cast<int64_t>(GL_RGBA8);
        sci.width = OVERLAY_WIDTH;
        sci.height = OVERLAY_HEIGHT;
        sci.mipCount = 1;
        sci.faceCount = 1;
        sci.sampleCount = 1;
        sci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;

        XrResult r = xrCreateSwapchain(Session, &sci, &textSwapchain);
        ALOG("main\noverlay: xrCreateSwapchain r=%d", r);
        if (XR_FAILED(r)) {
            ALOG("main\noverlay: ERROR xrCreateSwapchain failed: %d", r);
            return false;
        }
        return true;
    }

    bool InitializeGuiSystem() {
        GuiSys.reset(OVRFW::OvrGuiSys::Create(GetContext()));
        if (!GuiSys) {
            ALOG("main\noverlay: ERROR cannot create GuiSys");
            return false;
        }

        class DummySoundPlayer : public OVRFW::OvrGuiSys::SoundEffectPlayer {
        public:
            bool Has(const char* name) const override { (void)name; return false; }
            void Play(const char* name) override { (void)name; }
        };
        static DummySoundPlayer soundPlayer;

        GuiSys->Init(
                GetFileSys(),
                soundPlayer,
                "efigs.fnt",
                nullptr
        );

        return true;
    }

    void UpdateTextOverlay() {
        XrSwapchainImageAcquireInfo acq{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        uint32_t imageIndex = 0;
        XrResult r = xrAcquireSwapchainImage(textSwapchain, &acq, &imageIndex);
        if (XR_FAILED(r)) { ALOG("main\noverlay: ERROR xrAcquireSwapchainImage=%d", r); return; }

        XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        wait.timeout = XR_INFINITE_DURATION;
        r = xrWaitSwapchainImage(textSwapchain, &wait);
        if (XR_FAILED(r)) {
            ALOG("main\noverlay: ERROR xrWaitSwapchainImage=%d", r);
            XrSwapchainImageReleaseInfo rel{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            xrReleaseSwapchainImage(textSwapchain, &rel);
            return;
        }

        if (imageIndex >= textSwapchainImages.size()) {
            ALOG("main\noverlay: ERROR imageIndex OOB %u >= %zu", imageIndex, textSwapchainImages.size());
            XrSwapchainImageReleaseInfo rel{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            xrReleaseSwapchainImage(textSwapchain, &rel);
            return;
        }

        const GLuint tex = textSwapchainImages[imageIndex].image;
        ALOG("main\noverlay: acquired image=%u tex=%u", imageIndex, tex);
        RenderTextToTexture(tex);

        XrSwapchainImageReleaseInfo rel{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        xrReleaseSwapchainImage(textSwapchain, &rel);
    }

    void RenderTextToTexture(GLuint texture) {
        if (texture == 0) { ALOG("main\noverlay: WARN RenderTextToTexture no texture"); return; }

        if (framebuffer == 0) { glGenFramebuffers(1, &framebuffer); }

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            ALOG("main\noverlay: FBO incomplete 0x%x", status);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return;
        }

        glViewport(0, 0, OVERLAY_WIDTH, OVERLAY_HEIGHT);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        // Fondo semitransparente (debug: alfa alta para ver si cubre)
        glClearColor(0.0f, 0.0f, 0.0f, 0.85f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Texto con GuiSys sobre el FBO
        if (GuiSys) {
            OVRFW::BitmapFont& font = GuiSys->GetDefaultFont();
            OVRFW::BitmapFontSurface& fontSurface = GuiSys->GetDefaultFontSurface();

            OVRFW::fontParms_t fp;
            fp.AlignHoriz = OVRFW::HORIZONTAL_CENTER;
            fp.AlignVert  = OVRFW::VERTICAL_CENTER;
            fp.Billboard  = false; // 2D

            const OVR::Vector3f pos(OVERLAY_WIDTH * 0.50f, OVERLAY_HEIGHT * 0.50f, 0.0f);
            const float scale = 96.0f;
            const OVR::Vector4f color(1.0f, 1.0f, 1.0f, 1.0f);

            // 1) Intento 2D puro
            bool used2D = false;
#if defined(OVR_FONT_HAS_DRAW_TEXT_3DF)
            fontSurface.DrawText3Df(font, fp, pos, scale, color, currentTimeText.c_str());
            used2D = true;
#else
            // 2) Fallback: billboarded, pero con Billboard=false no rotará
            fontSurface.DrawTextBillboarded3Df(
                    font, fp, pos, scale, color, currentTimeText.c_str());
#endif

            // *** IMPORTANTE: primero Finish, luego AppendSurfaceList ***
            fontSurface.Finish(OVR::Matrix4f::Ortho2D((float)OVERLAY_WIDTH, (float)OVERLAY_HEIGHT));

            std::vector<OVRFW::ovrDrawSurface> surfaces;
            fontSurface.AppendSurfaceList(font, surfaces);

            ALOG("overlay: used2D=%d, surfaces=%d, pos=(%.1f,%.1f), scale=%.1f",
                 used2D ? 1 : 0, (int)surfaces.size(), pos.x, pos.y, scale);

            // Sondas de pixeles (centro + posición del texto)
            unsigned char pxR[4] = {0,0,0,0};
            glReadPixels((int)pos.x + 60, (int)pos.y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pxR);
            ALOG("overlay: rightOfText pixel RGBA=(%u,%u,%u,%u)", pxR[0],pxR[1],pxR[2],pxR[3]);

            unsigned char pxT[4] = {0,0,0,0};
            glReadPixels((int)pos.x, (int)pos.y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pxT);
            ALOG("overlay: textPos pixel RGBA=(%u,%u,%u,%u) at (%.0f,%.0f)", pxT[0],pxT[1],pxT[2],pxT[3], pos.x, pos.y);

            if (!surfaces.empty()) {
                // Fuerza el GpuState del surface del font
                OVRFW::ovrSurfaceDef* def = const_cast<OVRFW::ovrSurfaceDef*>(surfaces[0].surface);
                def->graphicsCommand.GpuState.depthEnable = false;
                def->graphicsCommand.GpuState.depthMaskEnable = false;
                def->graphicsCommand.GpuState.cullEnable = false;

                // Asegura alpha blend estándar
                def->graphicsCommand.GpuState.blendEnable = OVRFW::ovrGpuState::BLEND_ENABLE_SEPARATE;
                def->graphicsCommand.GpuState.blendSrc = GL_SRC_ALPHA;
                def->graphicsCommand.GpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;
                def->graphicsCommand.GpuState.blendSrcAlpha = GL_SRC_ALPHA;
                def->graphicsCommand.GpuState.blendDstAlpha = GL_ONE_MINUS_SRC_ALPHA;

                // Abre la máscara de color (por si el material del font la cerró)
                def->graphicsCommand.GpuState.colorMaskEnable[0] = true;
                def->graphicsCommand.GpuState.colorMaskEnable[1] = true;
                def->graphicsCommand.GpuState.colorMaskEnable[2] = true;
                def->graphicsCommand.GpuState.colorMaskEnable[3] = true;

                ALOG("overlay: forced GpuState on font surface (depth=0, cull=0, blend=SEPARATE, colorMask=1111)");

                // Matrices: Ortho en el UBO (dos vistas), eye=0
                const OVR::Matrix4f viewI = OVR::Matrix4f::Identity();
                const OVR::Matrix4f projI = OVR::Matrix4f::Identity();
                OVR::Matrix4f views[2] = { viewI, viewI };
                OVR::Matrix4f projs[2] = { projI, projI };

                auto ctrs = GetSurfaceRender().RenderSurfaceList(surfaces, views[0], projs[0], 0);
                ALOG("overlay: RSL draws=%d progBinds=%d texBinds=%d bufBinds=%d",
                     ctrs.numDrawCalls, ctrs.numProgramBinds, ctrs.numTextureBinds, ctrs.numBufferBinds);
            }else {
                ALOG("overlay: NO SURFACES (¿fuente/atlas?)");
            }
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }

    void SetupCompositorLayer() {
        textOverlayLayer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
        textOverlayLayer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
        textOverlayLayer.space = HeadSpace; // relativo a la cabeza
        textOverlayLayer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;

        // Posición/tamaño como el ejemplo funcional
        textOverlayLayer.pose.position.x = 0.4f;
        textOverlayLayer.pose.position.y = -0.3f;
        textOverlayLayer.pose.position.z = -1.5f;
        textOverlayLayer.pose.orientation.x = 0.0f;
        textOverlayLayer.pose.orientation.y = 0.0f;
        textOverlayLayer.pose.orientation.z = 0.0f;
        textOverlayLayer.pose.orientation.w = 1.0f;

        textOverlayLayer.size.width = 0.3f;
        textOverlayLayer.size.height = 0.08f;

        textOverlayLayer.subImage.swapchain = textSwapchain;
        textOverlayLayer.subImage.imageRect.offset.x = 0;
        textOverlayLayer.subImage.imageRect.offset.y = 0;
        textOverlayLayer.subImage.imageRect.extent.width = OVERLAY_WIDTH;
        textOverlayLayer.subImage.imageRect.extent.height = OVERLAY_HEIGHT;
        textOverlayLayer.subImage.imageArrayIndex = 0;

        ALOG("main\noverlay: layer setup done");
    }

    void CleanupSystems() {
        if (framebuffer != 0) { glDeleteFramebuffers(1, &framebuffer); framebuffer = 0; }
        if (GuiSys) { GuiSys->Shutdown(); GuiSys.reset(); }
    }
};

ENTRY_POINT(PosicionesApp)
