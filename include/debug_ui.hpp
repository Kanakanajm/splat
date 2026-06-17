#pragma once

#include <cstdint>
#include <vector>

class Camera;
struct GLFWwindow;

struct CaptureState {
    bool is_running = false;
    bool triggered  = false;

    // Per-method enable flags (independent checkboxes)
    bool capture_pt      = false;
    bool capture_pm      = false;
    bool capture_ss      = false;
    bool capture_vs      = false;
    bool capture_pvs     = false;
    bool capture_mitsuba = false;

    // Shared SPP checkpoint schedule (PT, PM, Mitsuba use the same list)
    int shared_max_spp         = 64;
    int shared_num_checkpoints = 6;

    // Path tracer params
    int pt_max_depth = 8;

    // Photon mapper params
    int   pm_n_photons      = 100000;
    float pm_r_surf         = 0.05f;
    float pm_r_vol          = 0.05f;
    int   pm_max_cam_depth  = 3;
    int   pm_max_emit_depth = 5;

    // Surface splat params
    int   ss_total_photons    = 1000000;
    int   ss_photons_per_pass = 100000;
    int   ss_max_emit_depth   = 20;
    float ss_h                = 0.01f;
    float ss_exposure         = 1.0f;
    int   ss_num_checkpoints  = 5;

    // Volume splat params
    int   vs_total_photons    = 1000000;
    int   vs_photons_per_pass = 100000;
    int   vs_max_emit_depth   = 20;
    float vs_beam_radius      = 0.05f;
    float vs_exposure         = 1.0f;
    int   vs_num_checkpoints  = 5;

    // Combined (PVS) splat params
    int   pvs_total_photons    = 1000000;
    int   pvs_photons_per_pass = 100000;
    int   pvs_max_emit_depth   = 20;
    float pvs_h                = 0.01f;
    float pvs_beam_radius      = 0.05f;
    float pvs_exposure         = 1.0f;
    int   pvs_num_checkpoints  = 5;

    // Mitsuba params (SPP comes from shared schedule)
    int mit_max_depth = 8;

    // Currently rendering method name (set by main loop, shown in UI)
    char current_method[64] = "";

    // Camera inspector
    char camera_json_path[256] = "";
    bool pending_camera_load   = false;
};

struct ViewState {
    // --- Geometry ------------------------------------------------------------
    bool showGeometry = false;
    bool useShadow    = true;
    enum class GeomAov : int { None, Diffuse, Normal, Depth, Backface } geomAov = GeomAov::None;
    std::vector<bool> instanceVisible;  // per-instance; empty = all visible

    // --- Photon Points -------------------------------------------------------
    bool showPoints = false;
    enum class PointAov : int { InstanceId, BsdfKind, BounceDepth,
                               PowerColor, PowerLuminance, PowerNormalized } pointAov = PointAov::InstanceId;
    std::vector<bool> instancePointsVisible;  // per-instance; empty = all visible
    bool allBounces         = true;   // show points from all bounce depths
    int  bounceFilter       = 0;      // active when allBounces == false
    bool showSplatTriangle  = false;  // draw splat triangle footprint instead of GL point

    // --- Pixel picker --------------------------------------------------------
    float pick_r = 0.0f, pick_g = 0.0f, pick_b = 0.0f;
    bool  has_pick = false;

    // --- Splat pass ----------------------------------------------------------
    float splatH     = 0.01f;
    float exposure   = 1.0f;
    enum class SplatAov : int { Radiance, Wireframe, Normal } splatAov = SplatAov::Radiance;

    // --- Photon Beams --------------------------------------------------------
    bool showBeams = false;
    enum class BeamAov : int { MediumId, T, BounceDepth, Length,
                               BeamPowerStart, BeamTransmittancePreview, Splat } beamAov = BeamAov::Splat;
    std::vector<bool> mediumBeamsVisible;  // per-medium; empty = all visible
    bool  allBeamBounces   = true;
    int   beamBounceFilter = 0;
    float beamRadius       = 0.05f;
    float beamExposure     = 1.0f;
};

class DebugUi {
public:
    explicit DebugUi(GLFWwindow* window);
    ~DebugUi();

    DebugUi(const DebugUi&) = delete;
    DebugUi& operator=(const DebugUi&) = delete;

    void beginFrame();
    // Draws all ImGui panels; returns true if vsync was toggled.
    bool draw(const Camera& camera, bool& vsyncEnabled,
              uint32_t instance_count, uint32_t medium_count,
              uint32_t max_bounce, uint32_t beam_max_bounce);
    void endFrame();

    bool wantsMouse() const;
    bool wantsKeyboard() const;

    void pick(float r, float g, float b);

    const ViewState& viewState()  const { return state_; }
    CaptureState&    captureState()     { return capture_; }

private:
    void drawGeometryPanel();
    void drawPhotonPointPanel(uint32_t max_bounce);
    void drawPhotonBeamPanel(uint32_t max_bounce);

    void drawCapturePanel(const Camera& camera);

    ViewState    state_;
    CaptureState capture_;
    bool showDemoWindow_ = false;
};
