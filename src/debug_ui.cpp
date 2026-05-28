#include "debug_ui.hpp"

#include "camera.hpp"

#include <GLFW/glfw3.h>
#include <cstdio>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

// ---- lifecycle --------------------------------------------------------------

DebugUi::DebugUi(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

DebugUi::~DebugUi() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void DebugUi::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void DebugUi::endFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool DebugUi::wantsMouse()    const { return ImGui::GetIO().WantCaptureMouse; }
bool DebugUi::wantsKeyboard() const { return ImGui::GetIO().WantCaptureKeyboard; }

void DebugUi::pick(float r, float g, float b) {
    state_.pick_r = r; state_.pick_g = g; state_.pick_b = b;
    state_.has_pick = true;
}

// ---- sub-panels -------------------------------------------------------------

void DebugUi::drawGeometryPanel() {
    if (!ImGui::CollapsingHeader("Geometry")) return;

    ImGui::Checkbox("Show geometry", &state_.showGeometry);
    if (!state_.showGeometry) return;

    ImGui::Text("AOV:");
    ImGui::SameLine();
    int g = static_cast<int>(state_.geomAov);
    ImGui::RadioButton("None",     &g, static_cast<int>(ViewState::GeomAov::None));    ImGui::SameLine();
    ImGui::RadioButton("Diffuse",  &g, static_cast<int>(ViewState::GeomAov::Diffuse)); ImGui::SameLine();
    ImGui::RadioButton("Normal",   &g, static_cast<int>(ViewState::GeomAov::Normal));  ImGui::SameLine();
    ImGui::RadioButton("Depth",    &g, static_cast<int>(ViewState::GeomAov::Depth));   ImGui::SameLine();
    ImGui::RadioButton("Backface", &g, static_cast<int>(ViewState::GeomAov::Backface));
    state_.geomAov = static_cast<ViewState::GeomAov>(g);
    ImGui::Checkbox("Shadow", &state_.useShadow);

    if (!state_.instanceVisible.empty()) {
        ImGui::Text("Instances:");
        for (int i = 0; i < static_cast<int>(state_.instanceVisible.size()); ++i) {
            char label[32];
            snprintf(label, sizeof(label), "##inst%d", i);
            bool v = state_.instanceVisible[i];
            if (ImGui::Checkbox(label, &v)) state_.instanceVisible[i] = v;
            ImGui::SameLine();
            ImGui::Text("%d", i);
            if ((i + 1) % 4 != 0) ImGui::SameLine();
        }
        ImGui::NewLine();
    }
}

void DebugUi::drawSplatPanel() {
    if (!ImGui::CollapsingHeader("Splat Pass")) return;
    ImGui::Checkbox("Show splat", &state_.showSplat);
    if (!state_.showSplat) return;
    ImGui::TextDisabled("Requires geometry pass to populate depth buffer.");
    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderFloat("h (bandwidth)", &state_.splatH, 0.001f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderFloat("Exposure", &state_.exposure, 0.01f, 100.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::Text("AOV:");
    ImGui::SameLine();
    int s = static_cast<int>(state_.splatAov);
    ImGui::RadioButton("Radiance",  &s, static_cast<int>(ViewState::SplatAov::Radiance));  ImGui::SameLine();
    ImGui::RadioButton("Wireframe", &s, static_cast<int>(ViewState::SplatAov::Wireframe)); ImGui::SameLine();
    ImGui::RadioButton("Normal",    &s, static_cast<int>(ViewState::SplatAov::Normal));
    state_.splatAov = static_cast<ViewState::SplatAov>(s);
}

void DebugUi::drawPhotonPointPanel(uint32_t max_bounce) {
    if (!ImGui::CollapsingHeader("Photon Points")) return;

    ImGui::Checkbox("Show points", &state_.showPoints);
    if (!state_.showPoints) return;

    ImGui::Text("AOV:");
    ImGui::SameLine();
    int p = static_cast<int>(state_.pointAov);
    ImGui::RadioButton("InstanceId",      &p, static_cast<int>(ViewState::PointAov::InstanceId));       ImGui::SameLine();
    ImGui::RadioButton("BsdfKind",        &p, static_cast<int>(ViewState::PointAov::BsdfKind));         ImGui::SameLine();
    ImGui::RadioButton("BounceDepth##pt", &p, static_cast<int>(ViewState::PointAov::BounceDepth));      ImGui::SameLine();
    ImGui::RadioButton("PowerColor",      &p, static_cast<int>(ViewState::PointAov::PowerColor));       ImGui::SameLine();
    ImGui::RadioButton("PowerLum",        &p, static_cast<int>(ViewState::PointAov::PowerLuminance));   ImGui::SameLine();
    ImGui::RadioButton("PowerNorm",       &p, static_cast<int>(ViewState::PointAov::PowerNormalized));
    state_.pointAov = static_cast<ViewState::PointAov>(p);

    ImGui::Checkbox("All bounces", &state_.allBounces);
    if (!state_.allBounces) {
        state_.bounceFilter = std::min(state_.bounceFilter, static_cast<int>(max_bounce));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::SliderInt("##bounce", &state_.bounceFilter, 0, static_cast<int>(max_bounce));
        ImGui::SameLine();
        ImGui::Text("bounce %d", state_.bounceFilter);
    }

    if (!state_.instancePointsVisible.empty()) {
        ImGui::Text("Filter by instance:");
        for (int i = 0; i < static_cast<int>(state_.instancePointsVisible.size()); ++i) {
            char label[32];
            snprintf(label, sizeof(label), "##ptinst%d", i);
            bool v = state_.instancePointsVisible[i];
            if (ImGui::Checkbox(label, &v)) state_.instancePointsVisible[i] = v;
            ImGui::SameLine();
            ImGui::Text("%d", i);
            if ((i + 1) % 4 != 0) ImGui::SameLine();
        }
        ImGui::NewLine();
    }
}

void DebugUi::drawPhotonBeamPanel(uint32_t max_bounce) {
    if (!ImGui::CollapsingHeader("Photon Beams")) return;

    ImGui::Checkbox("Show beams", &state_.showBeams);
    if (!state_.showBeams) return;

    ImGui::Text("AOV:");
    ImGui::SameLine();
    int b = static_cast<int>(state_.beamAov);
    ImGui::RadioButton("MediumId",        &b, static_cast<int>(ViewState::BeamAov::MediumId));              ImGui::SameLine();
    ImGui::RadioButton("T",               &b, static_cast<int>(ViewState::BeamAov::T));                    ImGui::SameLine();
    ImGui::RadioButton("BounceDepth##bm", &b, static_cast<int>(ViewState::BeamAov::BounceDepth));          ImGui::SameLine();
    ImGui::RadioButton("Length",          &b, static_cast<int>(ViewState::BeamAov::Length));               ImGui::SameLine();
    ImGui::RadioButton("Power",           &b, static_cast<int>(ViewState::BeamAov::BeamPowerStart));       ImGui::SameLine();
    ImGui::RadioButton("Transmittance",   &b, static_cast<int>(ViewState::BeamAov::BeamTransmittancePreview));
    state_.beamAov = static_cast<ViewState::BeamAov>(b);

    ImGui::Checkbox("All bounces##bm", &state_.allBeamBounces);
    if (!state_.allBeamBounces) {
        state_.beamBounceFilter = std::min(state_.beamBounceFilter, static_cast<int>(max_bounce));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::SliderInt("##beambounce", &state_.beamBounceFilter, 0, static_cast<int>(max_bounce));
        ImGui::SameLine();
        ImGui::Text("bounce %d", state_.beamBounceFilter);
    }

    if (!state_.mediumBeamsVisible.empty()) {
        ImGui::Text("Filter by medium:");
        for (int i = 0; i < static_cast<int>(state_.mediumBeamsVisible.size()); ++i) {
            char label[32];
            snprintf(label, sizeof(label), "##med%d", i);
            bool v = state_.mediumBeamsVisible[i];
            if (ImGui::Checkbox(label, &v)) state_.mediumBeamsVisible[i] = v;
            ImGui::SameLine();
            ImGui::Text("%d", i);
            if ((i + 1) % 4 != 0) ImGui::SameLine();
        }
        ImGui::NewLine();
    }
}

// ---- main draw --------------------------------------------------------------

bool DebugUi::draw(const Camera& camera, bool& vsyncEnabled,
                   uint32_t instance_count, uint32_t medium_count,
                   uint32_t max_bounce, uint32_t beam_max_bounce) {
    if (showDemoWindow_) ImGui::ShowDemoWindow(&showDemoWindow_);

    // Resize filter vectors to match scene counts (fill new slots as visible).
    state_.instanceVisible.resize(instance_count, true);
    state_.instancePointsVisible.resize(instance_count, true);
    state_.mediumBeamsVisible.resize(medium_count, true);

    const ImGuiIO& io = ImGui::GetIO();

    ImGui::Begin("Debug");

    // Info
    ImGui::Text("FPS: %.1f", io.Framerate);
    const bool vsyncChanged = ImGui::Checkbox("VSync", &vsyncEnabled);
    ImGui::Separator();
    ImGui::Text("Camera: (%.2f, %.2f, %.2f)  pitch %.1f  yaw %.1f",
                camera.Position.x, camera.Position.y, camera.Position.z,
                camera.Pitch, camera.Yaw);
    ImGui::Separator();

    // Pixel picker
    if (state_.has_pick) {
        ImGui::ColorButton("##pick",
            ImVec4{state_.pick_r, state_.pick_g, state_.pick_b, 1.0f},
            ImGuiColorEditFlags_NoTooltip, ImVec2(16, 16));
        ImGui::SameLine();
        ImGui::Text("R %.3f  G %.3f  B %.3f", state_.pick_r, state_.pick_g, state_.pick_b);
    } else {
        ImGui::TextDisabled("Left-click viewport to sample pixel");
    }
    ImGui::Separator();

    drawGeometryPanel();
    drawSplatPanel();
    drawPhotonPointPanel(max_bounce);
    drawPhotonBeamPanel(beam_max_bounce);

    ImGui::Separator();
    ImGui::Checkbox("ImGui demo", &showDemoWindow_);

    ImGui::End();

    return vsyncChanged;
}
