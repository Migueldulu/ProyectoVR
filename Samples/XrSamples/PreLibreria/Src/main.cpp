#include <string>
#include <string_view>
#include <fstream>
#include <chrono>
#include <vector>
#include <sstream>
#include <iomanip>

#include <openxr/openxr.h>

#include "GUI/VRMenuObject.h"
#include "Render/BitmapFont.h"
#include "XrApp.h"

#include "OVR_Math.h"
#include "Input/ControllerRenderer.h"
#include "Input/TinyUI.h"
#include "Render/SimpleBeamRenderer.h"

// Clase para manejar la grabación de movimientos
class MovementRecorder {
public:
    // Estructura para almacenar los datos de cada frame (ahora dentro de la clase)
    struct FrameData {
        double timestamp;

        // Headset
        float headPosX, headPosY, headPosZ;
        float headRotX, headRotY, headRotZ, headRotW;

        // Controlador izquierdo
        bool leftControllerTracked;
        float leftPosX, leftPosY, leftPosZ;
        float leftRotX, leftRotY, leftRotZ, leftRotW;
        float leftTriggerValue;

        // Controlador derecho
        bool rightControllerTracked;
        float rightPosX, rightPosY, rightPosZ;
        float rightRotX, rightRotY, rightRotZ, rightRotW;
        float rightTriggerValue;

        // Botones
        bool buttonA_pressed;

        // Constructor para inicializar con datos del frame
        FrameData(const OVRFW::ovrApplFrameIn& in, double ts) : timestamp(ts) {
            // Headset
            headPosX = in.HeadPose.Translation.x;
            headPosY = in.HeadPose.Translation.y;
            headPosZ = in.HeadPose.Translation.z;
            headRotX = in.HeadPose.Rotation.x;
            headRotY = in.HeadPose.Rotation.y;
            headRotZ = in.HeadPose.Rotation.z;
            headRotW = in.HeadPose.Rotation.w;

            // Controlador izquierdo
            leftControllerTracked = in.LeftRemoteTracked;
            if (leftControllerTracked) {
                leftPosX = in.LeftRemotePose.Translation.x;
                leftPosY = in.LeftRemotePose.Translation.y;
                leftPosZ = in.LeftRemotePose.Translation.z;
                leftRotX = in.LeftRemotePose.Rotation.x;
                leftRotY = in.LeftRemotePose.Rotation.y;
                leftRotZ = in.LeftRemotePose.Rotation.z;
                leftRotW = in.LeftRemotePose.Rotation.w;
                leftTriggerValue = in.LeftRemoteIndexTrigger;
            } else {
                leftPosX = leftPosY = leftPosZ = 0.0f;
                leftRotX = leftRotY = leftRotZ = 0.0f;
                leftRotW = 1.0f;
                leftTriggerValue = 0.0f;
            }

            // Controlador derecho
            rightControllerTracked = in.RightRemoteTracked;
            if (rightControllerTracked) {
                rightPosX = in.RightRemotePose.Translation.x;
                rightPosY = in.RightRemotePose.Translation.y;
                rightPosZ = in.RightRemotePose.Translation.z;
                rightRotX = in.RightRemotePose.Rotation.x;
                rightRotY = in.RightRemotePose.Rotation.y;
                rightRotZ = in.RightRemotePose.Rotation.z;
                rightRotW = in.RightRemotePose.Rotation.w;
                rightTriggerValue = in.RightRemoteIndexTrigger;
            } else {
                rightPosX = rightPosY = rightPosZ = 0.0f;
                rightRotX = rightRotY = rightRotZ = 0.0f;
                rightRotW = 1.0f;
                rightTriggerValue = 0.0f;
            }

            // Botones
            buttonA_pressed = in.Clicked(OVRFW::ovrApplFrameIn::kButtonA);
        }

        // Convertir a CSV
        std::string toCSV() const {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(6)
                << timestamp << ","
                << headPosX << "," << headPosY << "," << headPosZ << ","
                << headRotX << "," << headRotY << "," << headRotZ << "," << headRotW << ","
                << (leftControllerTracked ? "1" : "0") << ","
                << leftPosX << "," << leftPosY << "," << leftPosZ << ","
                << leftRotX << "," << leftRotY << "," << leftRotZ << "," << leftRotW << ","
                << leftTriggerValue << ","
                << (rightControllerTracked ? "1" : "0") << ","
                << rightPosX << "," << rightPosY << "," << rightPosZ << ","
                << rightRotX << "," << rightRotY << "," << rightRotZ << "," << rightRotW << ","
                << rightTriggerValue << ","
                << (buttonA_pressed ? "1" : "0");
            return oss.str();
        }
    };
private:
    std::vector<FrameData> frameBuffer;
    std::chrono::high_resolution_clock::time_point startTime;
    int currentFileIndex;
    int frameCount;
    static const int MAX_FRAMES_PER_FILE = 5400; // 60 segundos a 90fps
    std::string baseFilename;

    std::string getCurrentFilename() const {
        std::ostringstream oss;
        oss << baseFilename << "_part" << std::setfill('0') << std::setw(3) << currentFileIndex << ".csv";
        return oss.str();
    }

    void saveBufferToFile() {
        if (frameBuffer.empty()) return;

        std::string filename = getCurrentFilename();
        std::ofstream file(filename);

        if (file.is_open()) {
            // Escribir cabecera CSV
            file << "timestamp,head_pos_x,head_pos_y,head_pos_z,"
                 << "head_rot_x,head_rot_y,head_rot_z,head_rot_w,"
                 << "left_tracked,left_pos_x,left_pos_y,left_pos_z,"
                 << "left_rot_x,left_rot_y,left_rot_z,left_rot_w,left_trigger,"
                 << "right_tracked,right_pos_x,right_pos_y,right_pos_z,"
                 << "right_rot_x,right_rot_y,right_rot_z,right_rot_w,right_trigger,"
                 << "button_a\n";

            // Escribir datos
            for (const auto& frame : frameBuffer) {
                file << frame.toCSV() << "\n";
            }

            file.close();
            ALOG("Saved %zu frames to %s", frameBuffer.size(), filename.c_str());

            // Simular envío a servidor web (URL falsa)
            simulateWebUpload(filename);
        } else {
            ALOG("Error: Could not open file %s for writing", filename.c_str());
        }
    }

    void simulateWebUpload(const std::string& filename) {
        // Simulación de envío a servidor web
        std::string fakeUrl = "https://api.vrmotion.fake/upload/session123/" + filename;
        ALOG("Simulating upload to: %s", fakeUrl.c_str());

        // Aquí irían las llamadas HTTP reales cuando implementes la funcionalidad
        // Por ejemplo usando libcurl o similar:
        // curl_easy_setopt(curl, CURLOPT_URL, fakeUrl.c_str());
        // curl_easy_setopt(curl, CURLOPT_POSTFIELDS, file_data);
        // curl_easy_perform(curl);
    }

public:
    MovementRecorder() : currentFileIndex(0), frameCount(0) {
        // Generar nombre base único con timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);

        std::ostringstream oss;
        oss << "vr_motion_"
            << std::put_time(&tm, "%Y%m%d_%H%M%S");
        baseFilename = oss.str();

        startTime = std::chrono::high_resolution_clock::now();
        frameBuffer.reserve(MAX_FRAMES_PER_FILE);

        ALOG("MovementRecorder initialized with base filename: %s", baseFilename.c_str());
    }

    void recordFrame(const OVRFW::ovrApplFrameIn& in) {
        auto now = std::chrono::high_resolution_clock::now();
        double timestamp = std::chrono::duration<double>(now - startTime).count();

        frameBuffer.emplace_back(in, timestamp);
        frameCount++;

        // Si el buffer está lleno, guardar archivo y empezar uno nuevo
        if (frameBuffer.size() >= MAX_FRAMES_PER_FILE) {
            saveBufferToFile();
            frameBuffer.clear();
            currentFileIndex++;
        }
    }

    void finalize() {
        // Guardar cualquier frame restante en el buffer
        if (!frameBuffer.empty()) {
            saveBufferToFile();
            frameBuffer.clear();
        }

        ALOG("MovementRecorder finalized. Total frames recorded: %d across %d files",
             frameCount, currentFileIndex + 1);
    }

    int getTotalFrames() const { return frameCount; }
    int getCurrentFileIndex() const { return currentFileIndex; }
};

class XrAppBaseApp : public OVRFW::XrApp {

private:
    OVRFW::VRMenuObject* holaMundoLabel;
    OVRFW::VRMenuObject* toggleButton;
    OVRFW::VRMenuObject* recordingStatusLabel;
    bool debeReposicionar = false;
    bool labelCreado = false;
    bool labelVisible = true;

    // Sistema de grabación
    MovementRecorder recorder;

public:

    XrAppBaseApp() : OVRFW::XrApp() {
        BackgroundColor = OVR::Vector4f(0.55f, 0.35f, 0.1f, 1.0f);
    }


    virtual std::vector<const char*> GetExtensions() override {
        std::vector<const char*> extensions = XrApp::GetExtensions();
        return extensions;
    }

    /* Antes de esta función, OVRFW::XrApp::Init() llama:
     - xrInitializeLoaderKHR
     - xrCreateInstance con las extensiones de GetExtensions,
     - xrSuggestInteractionProfileBindings(...) para configurar los enlaces de acciones */
    virtual bool AppInit(const xrJava* context) override {

        if (false == ui_.Init(context, GetFileSys())) {
            ALOG("TinyUI::Init FAILED.");
            return false;
        }

        ALOG("VR Motion Recording started automatically");
        return true;
    }

    /* Antes de esta función y dsp de AppInit,  XrApp::InitSession() llama:
     - xrCreateSession
     - xrCreateReferenceSpace para espacio local y espacio de escenario
     - Create swapchain with xrCreateSwapchain
     - xrAttachSessionActionSets */
    virtual bool SessionInit() override {
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

    virtual void Update(const OVRFW::ovrApplFrameIn& in) override {

        // NUEVO: Grabar datos de movimiento cada frame automáticamente
        recorder.recordFrame(in);

        if(!labelCreado){
            //Se obtiene la matriz de pos de la cabeza y se le añade un offset
            OVR::Matrix4f matrizInicial = OVR::Matrix4f(in.HeadPose);
            OVR::Vector3f posiInicial = matrizInicial.Transform({0.0f, -0.35f, -2.0f});
            holaMundoLabel = ui_.AddLabel("Super Hola Mundo", posiInicial, {400.0f, 100.0f});
            holaMundoLabel->SetLocalRotation(in.HeadPose.Rotation);

            //creamos el boton
            // NUEVO: Crear el botón virtual de toggle
            OVR::Vector3f posicionBoton = matrizInicial.Transform({-0.75f, -0.1f, -2.0f});
            toggleButton = ui_.AddButton(
                    "Ocultar Texto",
                    posicionBoton,
                    {200.0f, 75.0f},
                    [this]() {
                        this->ToggleTextoVisibilidad();
                    }
            );
            toggleButton->SetLocalRotation(in.HeadPose.Rotation);

            // NUEVO: Label para mostrar estado de grabación
            OVR::Vector3f posicionEstado = matrizInicial.Transform({0.0f, 0.15f, -2.0f});
            recordingStatusLabel = ui_.AddLabel("GRABANDO MOVIMIENTOS", posicionEstado, {350.0f, 60.0f});
            recordingStatusLabel->SetLocalRotation(in.HeadPose.Rotation);
            recordingStatusLabel->SetTextColor(OVR::Vector4f(1.0f, 0.2f, 0.2f, 1.0f)); // Rojo para indicar grabación

            labelCreado = true;
        }

        // Actualizar el texto del estado de grabación
        if (recordingStatusLabel) {
            std::ostringstream statusText;
            statusText << "GRABANDO: " << recorder.getTotalFrames()
                       << " frames | Archivo: " << (recorder.getCurrentFileIndex() + 1);
            recordingStatusLabel->SetText(statusText.str().c_str());
        }

        if (in.Clicked(OVRFW::ovrApplFrameIn::kButtonA)) {
            debeReposicionar = true;
            // Cambiar color temporalmente para feedback
            holaMundoLabel->SetTextColor(OVR::Vector4f(0.0f, 0.0f, 0.0f, 1.0f));
        } else {
            holaMundoLabel->SetTextColor(OVR::Vector4f(1.0f, 1.0f, 1.0f, 1.0f));
        }

        if (debeReposicionar) {
            ReposicionarElementos(in.HeadPose);
            debeReposicionar = false;
        }

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

        cursorBeamRenderer_.Render(in, out);
    }

    virtual void SessionEnd() override {
        controllerRenderL_.Shutdown();
        controllerRenderR_.Shutdown();
        cursorBeamRenderer_.Shutdown();
    }

    virtual void AppShutdown(const xrJava* context) override {
        // NUEVO: Finalizar la grabación cuando se cierre la app
        recorder.finalize();
        ALOG("VR Motion Recording finalized");

        OVRFW::XrApp::AppShutdown(context);
        ui_.Shutdown();
    }

private:
    OVRFW::ControllerRenderer controllerRenderL_;
    OVRFW::ControllerRenderer controllerRenderR_;
    OVRFW::TinyUI ui_;

    OVRFW::SimpleBeamRenderer cursorBeamRenderer_;

    void ToggleTextoVisibilidad() {
        if (holaMundoLabel != nullptr) {
            labelVisible = !labelVisible;
            holaMundoLabel->SetVisible(labelVisible);

            if (labelVisible) {
                toggleButton->SetText("Ocultar Texto");
            } else {
                toggleButton->SetText("Mostrar Texto");
            }
        }
        ALOG("ToggleTextoVisibilidad llamado - labelVisible: %s",
             labelVisible ? "true" : "false");
    }

    void ReposicionarElementos(const OVR::Posef& headPose) {
        if (holaMundoLabel && toggleButton && recordingStatusLabel) {
            ui_.RemoveParentMenu(recordingStatusLabel);
            ui_.RemoveParentMenu(toggleButton);
            ui_.RemoveParentMenu(holaMundoLabel);
            recordingStatusLabel = nullptr;
            toggleButton = nullptr;
            holaMundoLabel = nullptr;

            OVR::Matrix4f matrizNuevaCabeza = OVR::Matrix4f(headPose);

            OVR::Vector3f nuevaPosi = matrizNuevaCabeza.Transform({0.0f, -0.35f, -2.0f});
            holaMundoLabel = ui_.AddLabel("Super Hola Mundo", nuevaPosi, {400.0f, 100.0f});
            holaMundoLabel->SetLocalRotation(headPose.Rotation);
            holaMundoLabel->SetVisible(labelVisible);

            OVR::Vector3f posicionBoton = matrizNuevaCabeza.Transform({-0.75f, -0.1f, -2.0f});
            toggleButton = ui_.AddButton(
                    // si es true el 1 si es false el segundo
                    labelVisible ? "Ocultar Texto" : "Mostrar Texto",
                    posicionBoton,
                    {200.0f, 75.0f},
                    [this]() {
                        this->ToggleTextoVisibilidad();
                    }
            );
            toggleButton->SetLocalRotation(headPose.Rotation);

            // Recrear el label de estado de grabación
            OVR::Vector3f posicionEstado = matrizNuevaCabeza.Transform({0.0f, 0.15f, -2.0f});
            recordingStatusLabel = ui_.AddLabel("GRABANDO MOVIMIENTOS", posicionEstado, {350.0f, 60.0f});
            recordingStatusLabel->SetLocalRotation(headPose.Rotation);
            recordingStatusLabel->SetTextColor(OVR::Vector4f(1.0f, 0.2f, 0.2f, 1.0f)); // Rojo para indicar grabación
        }
    }
};

ENTRY_POINT(XrAppBaseApp)