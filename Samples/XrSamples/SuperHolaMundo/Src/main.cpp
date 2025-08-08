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

private:
    OVRFW::VRMenuObject* holaMundoLabel;  // La voy a usar para cambiar la posicion OJO es una ref no un obj
    bool debeReposicionar = false;  // Flag para reposicionar
    bool labelCreado = false;

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

        // Init objects that need OpenXR Session
        if (!controllerRenderL_.Init(true)) {
            ALOG("SessionInit::Init L controller renderer FAILED.");
            return false;
        }
        if (!controllerRenderR_.Init(false)) {
            ALOG("SessionInit::Init R controller renderer FAILED.");
            return false;
        }
        cursorBeamRenderer_.Init(GetFileSys(), nullptr, OVR::Vector4f(1.0f), 1.0f);

        return true;
    }

    // Se invoca cada frame
    virtual void Update(const OVRFW::ovrApplFrameIn& in) override {

        if(!labelCreado){
            //Se obtiene la matriz de pos de la cabeza y se le añade un offset
            OVR::Matrix4f matrizInicial = OVR::Matrix4f(in.HeadPose);
            OVR::Vector3f posiInicial = matrizInicial.Transform({0.0f, -0.35f, -2.0f});
            holaMundoLabel = ui_.AddLabel("Super Hola Mundo", posiInicial, {400.0f, 100.0f});
            holaMundoLabel->SetLocalRotation(in.HeadPose.Rotation);
            labelCreado = true;
        }
        // Detectar si se presiona botón A para reposicionar (como el botón Meta)
        if (in.Clicked(OVRFW::ovrApplFrameIn::kButtonA)) {
            debeReposicionar = true;
            // Cambiar color temporalmente para feedback
            holaMundoLabel->SetTextColor(OVR::Vector4f(0.0f, 0.0f, 0.0f, 1.0f));
        } else {
            holaMundoLabel->SetTextColor(OVR::Vector4f(1.0f, 1.0f, 1.0f, 1.0f));
        }

        if (debeReposicionar) {
            //borramos el padre, que es lo que tomaba setLocalPosition de referencia de posicion
            ui_.RemoveParentMenu(holaMundoLabel);
            holaMundoLabel = nullptr;
            OVR::Matrix4f matrizNuevaCabeza = OVR::Matrix4f(in.HeadPose);
            OVR::Vector3f nuevaPosi = matrizNuevaCabeza.Transform({0.0f, -0.35f, -2.0f});
            holaMundoLabel = ui_.AddLabel("Super Hola Mundo", nuevaPosi, {400.0f, 100.0f});
            holaMundoLabel->SetLocalRotation(in.HeadPose.Rotation);

            debeReposicionar = false;
        }

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