#include "debug_ui.hpp"

#include "camera.hpp"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <iomanip>
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
    std::cout << std::setprecision(8) << "Color of Picked Pixel: " << r << ", " << g << ", " << b << std::endl;
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
    ImGui::RadioButton("Transmittance",   &b, static_cast<int>(ViewState::BeamAov::BeamTransmittancePreview)); ImGui::SameLine();
    ImGui::RadioButton("Splat",           &b, static_cast<int>(ViewState::BeamAov::Splat));
    state_.beamAov = static_cast<ViewState::BeamAov>(b);

    ImGui::SetNextItemWidth(200.0f);
    ImGui::SliderFloat("Beam radius", &state_.beamRadius, 0.001f, 0.5f, "%.3f");

    const int bAov = static_cast<int>(state_.beamAov);
    if (bAov >= static_cast<int>(ViewState::BeamAov::BeamPowerStart)) {
        ImGui::SetNextItemWidth(200.0f);
        ImGui::SliderFloat("Exposure", &state_.beamExposure, 0.01f, 100.0f, "%.2f");
    }

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

void DebugUi::drawCapturePanel(const Camera& camera) {
    if (!ImGui::CollapsingHeader("Capture")) return;

    // Render mode radio
    int mode = capture_.use_path_tracer ? 1 : (capture_.use_photon_mapper ? 2 :
               (capture_.use_surface_splatter ? 3 : 0));
    bool mode_changed = ImGui::RadioButton("Beam Splat##mode",    &mode, 0); ImGui::SameLine();
    mode_changed |=     ImGui::RadioButton("Path Tracer##mode",   &mode, 1); ImGui::SameLine();
    mode_changed |=     ImGui::RadioButton("Photon Mapper##mode", &mode, 2); ImGui::SameLine();
    mode_changed |=     ImGui::RadioButton("Surface Splat##mode", &mode, 3);
    if (mode_changed) {
        capture_.use_path_tracer      = (mode == 1);
        capture_.use_photon_mapper    = (mode == 2);
        capture_.use_surface_splatter = (mode == 3);
    }

    ImGui::Separator();
    if (mode == 0) {
        // Beam splat mode params
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputInt("Total photons",    &capture_.total_photons);
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputInt("Photons per pass", &capture_.photons_per_pass);
    } else if (mode == 3) {
        // Surface splat mode params
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputInt("Total photons",    &capture_.total_photons);
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputInt("Photons per pass", &capture_.photons_per_pass);
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputInt("Max emit depth",   &capture_.ss_max_emit_depth);
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputFloat("Bandwidth h",    &capture_.ss_h, 0.001f, 0.05f, "%.4f");
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputFloat("Exposure",       &capture_.ss_exposure, 0.1f, 1.0f, "%.2f");

        ImGui::Separator();
        ImGui::Checkbox("Compare with PM", &capture_.ss_compare_pm);
        if (capture_.ss_compare_pm) {
            ImGui::TextDisabled("PM uses r_surf=h, r_vol=pm_r_vol, n=%d", capture_.pm_n_photons);
            ImGui::SetNextItemWidth(160.0f);
            ImGui::InputInt("PM SPP",       &capture_.ss_pm_spp);
            ImGui::SetNextItemWidth(160.0f);
            ImGui::InputInt("Checkpoints",  &capture_.ss_num_checkpoints);
            ImGui::Checkbox("PM checkpoints", &capture_.ss_pm_save_checkpoints);
            if (!capture_.ss_pm_save_checkpoints)
                ImGui::SameLine(), ImGui::TextDisabled("(final only)");
        }
    } else if (mode == 1) {
        // Path tracer params
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputInt("SPP",       &capture_.pt_spp);
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputInt("Max depth", &capture_.pt_max_depth);
    } else {
        // Photon mapper params
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputInt("N photons",      &capture_.pm_n_photons);
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputFloat("r surf",       &capture_.pm_r_surf, 0.005f, 0.1f, "%.4f");
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputFloat("r vol",        &capture_.pm_r_vol,  0.005f, 0.1f, "%.4f");
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputInt("Max cam depth",  &capture_.pm_max_cam_depth);
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputInt("Max emit depth", &capture_.pm_max_emit_depth);
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputInt("SPP",            &capture_.pm_spp);
    }

    // Reference comparison (PT and PM only)
    if (mode == 1 || mode == 2) {
        ImGui::Separator();
        ImGui::Text("Compare reference:");
        ImGui::RadioButton("None##ref",        &capture_.compare_reference, 0); ImGui::SameLine();
        ImGui::RadioButton("Mitsuba##ref",     &capture_.compare_reference, 1); ImGui::SameLine();
        ImGui::RadioButton("Path Tracer##ref", &capture_.compare_reference, 2);
        if (capture_.compare_reference > 0) {
            ImGui::SetNextItemWidth(120.0f);
            if (mode == 1) {
                ImGui::InputInt("Checkpoints", &capture_.pt_num_checkpoints);
            } else {
                ImGui::InputInt("Checkpoints",   &capture_.pm_num_checkpoints);
                ImGui::SetNextItemWidth(120.0f);
                ImGui::InputInt("Compare depth",  &capture_.pm_compare_depth);
                ImGui::SameLine();
                ImGui::TextDisabled("(emit+cam = %d)",
                    capture_.pm_max_emit_depth + capture_.pm_max_cam_depth);
            }
        }
    }

    if (mode == 0 || (mode == 3 && !capture_.ss_compare_pm) ||
        ((mode == 1 || mode == 2) && capture_.compare_reference == 0)) {
        ImGui::SetNextItemWidth(240.0f);
        ImGui::InputText("Output path", capture_.output_path, sizeof(capture_.output_path));
    }

    if (capture_.is_running) {
        ImGui::BeginDisabled();
        ImGui::Button("Rendering...");
        ImGui::EndDisabled();
    } else {
        if (ImGui::Button("Render")) {
            if (mode == 1) {
                capture_.pt_spp             = std::max(1, capture_.pt_spp);
                capture_.pt_max_depth       = std::max(1, capture_.pt_max_depth);
                capture_.pt_num_checkpoints = std::max(1, capture_.pt_num_checkpoints);
            } else if (mode == 2) {
                capture_.pm_n_photons       = std::max(1, capture_.pm_n_photons);
                capture_.pm_max_cam_depth   = std::max(1, capture_.pm_max_cam_depth);
                capture_.pm_max_emit_depth  = std::max(1, capture_.pm_max_emit_depth);
                capture_.pm_spp             = std::max(1, capture_.pm_spp);
                capture_.pm_num_checkpoints = std::max(1, capture_.pm_num_checkpoints);
            } else if (mode == 3) {
                capture_.total_photons     = std::max(1, capture_.total_photons);
                capture_.photons_per_pass  = std::max(1, capture_.photons_per_pass);
                capture_.ss_max_emit_depth = std::max(1, capture_.ss_max_emit_depth);
                capture_.ss_h              = std::max(1e-4f, capture_.ss_h);
                capture_.ss_exposure       = std::max(1e-4f, capture_.ss_exposure);
                if (capture_.ss_compare_pm) {
                    capture_.ss_pm_spp          = std::max(1, capture_.ss_pm_spp);
                    capture_.ss_num_checkpoints = std::max(1, capture_.ss_num_checkpoints);
                }
            } else {
                capture_.total_photons    = std::max(1, capture_.total_photons);
                capture_.photons_per_pass = std::max(1, capture_.photons_per_pass);
            }
            capture_.triggered = true;
        }
    }

    // Camera inspector
    ImGui::Separator();
    if (ImGui::TreeNode("Camera")) {
        ImGui::Text("pos (%.3f, %.3f, %.3f)", camera.Position.x, camera.Position.y, camera.Position.z);
        ImGui::Text("dir (%.3f, %.3f, %.3f)", camera.Front.x,    camera.Front.y,    camera.Front.z);
        ImGui::Text("fov %.1f deg  yaw %.1f  pitch %.1f", camera.Zoom, camera.Yaw, camera.Pitch);
        ImGui::SetNextItemWidth(240.0f);
        ImGui::InputText("JSON path", capture_.camera_json_path, sizeof(capture_.camera_json_path));
        ImGui::SameLine();
        if (ImGui::Button("Load")) capture_.pending_camera_load = true;
        ImGui::TreePop();
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
    drawCapturePanel(camera);

    ImGui::Separator();
    ImGui::Checkbox("ImGui demo", &showDemoWindow_);

    ImGui::End();

    return vsyncChanged;
}
