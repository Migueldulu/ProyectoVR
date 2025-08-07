#include <string>
#include <string_view>

#include <openxr/openxr.h>

#include "GUI/VRMenuObject.h"
#include "Render/BitmapFont.h"
#include "XrApp.h"

#include "OVR_Math.h"
#include "Input/ControllerRenderer.h"
#include "Input/TinyUI.h"
#include "Render/SimpleBeamRenderer.h"

class XrAppBaseApp : public OVRFW::XrApp {
public:
    // Constructor, establece color de fondo
    XrAppBaseApp() : OVRFW::XrApp() {
        BackgroundColor = OVR::Vector4f(0.55f, 0.35f, 0.1f, 1.0f);
    }

    // Lista de extensiones necesarias para esta app
    virtual std::vector<const char*> GetExtensions() override {
        std::vector<const char*> extensions = XrApp::GetExtensions();
        return extensions;
    }

    // Antes de esta función, OVRFW::XrApp::Init() llama:
    // - xrInitializeLoaderKHR
    // - xrCreateInstance con las extensiones de GetExtensions,
    // - xrSuggestInteractionProfileBindings(...) para configurar los enlaces de acciones
    virtual bool AppInit(const xrJava* context) override {
        // Init UI system
        if (false == ui_.Init(context, GetFileSys())) {
            ALOG("TinyUI::Init FAILED.");
            return false;
        }
        return true;
    }

    // Antes de esta función y dsp de AppInit,  XrApp::InitSession() llama:
    // - xrCreateSession
    // - xrCreateReferenceSpace para espacio local y espacio de escenario
    // - Create swapchain with xrCreateSwapchain
    // - xrAttachSessionActionSets
    virtual bool SessionInit() override {
        // Crear el mensaje "Hola Mundo" en el centro de la vista
        ui_.AddLabel("Hola Mundo", {0.0f, 1.5f, -2.0f}, {400.0f, 100.0f});

        // Init objects that need OpenXR Session
        if (false == controllerRenderL_.Init(true)) {
            ALOG("SessionInit::Init L controller renderer FAILED.");
            return false;
        }
        if (false == controllerRenderR_.Init(false)) {
            ALOG("SessionInit::Init R controller renderer FAILED.");
            return false;
        }
        cursorBeamRenderer_.Init(GetFileSys(), nullptr, OVR::Vector4f(1.0f), 1.0f);

        return true;
    }

    // Se invoca cada frame
    virtual void Update(const OVRFW::ovrApplFrameIn& in) override {
        // Clear the intersection rays from last frame:
        ui_.HitTestDevices().clear();

        if (in.LeftRemoteTracked) {
            controllerRenderL_.Update(in.LeftRemotePose);
            const bool didTrigger = in.LeftRemoteIndexTrigger > 0.5f;
            ui_.AddHitTestRay(in.LeftRemotePointPose, didTrigger);
        }

        if (in.RightRemoteTracked) {
            controllerRenderR_.Update(in.RightRemotePose);
            const bool didTrigger = in.RightRemoteIndexTrigger > 0.5f;
            ui_.AddHitTestRay(in.RightRemotePointPose, didTrigger);
        }

        ui_.Update(in);
        cursorBeamRenderer_.Update(in, ui_.HitTestDevices());
    }

    virtual void Render(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out) override {
        ui_.Render(in, out);

        if (in.LeftRemoteTracked) {
            controllerRenderL_.Render(out.Surfaces);
        }
        if (in.RightRemoteTracked) {
            controllerRenderR_.Render(out.Surfaces);
        }

        /// Render beams last, since they render with transparency
        cursorBeamRenderer_.Render(in, out);
    }

    virtual void SessionEnd() override {
        controllerRenderL_.Shutdown();
        controllerRenderR_.Shutdown();
        cursorBeamRenderer_.Shutdown();
    }

    virtual void AppShutdown(const xrJava* context) override {
        OVRFW::XrApp::AppShutdown(context);
        ui_.Shutdown();
    }

private:
    OVRFW::ControllerRenderer controllerRenderL_;
    OVRFW::ControllerRenderer controllerRenderR_;
    OVRFW::TinyUI ui_;

    // Renderer that draws the beam from the controller
    OVRFW::SimpleBeamRenderer cursorBeamRenderer_;
};

ENTRY_POINT(XrAppBaseApp)