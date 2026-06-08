#pragma once

#include <cstdint>
#include <vector>

class Camera;
struct GLFWwindow;

struct CaptureState {
    int  total_photons    = 1000000;
    int  photons_per_pass = 100000;
    char output_path[256] = "output.exr";
    bool is_running       = false;
    bool triggered        = false;

    // Path tracer mode
    bool use_path_tracer          = false;
    int  pt_spp                   = 64;
    int  pt_max_depth             = 8;

    // Mitsuba comparison (PT mode only)
    bool compare_with_mitsuba     = false;
    int  pt_num_checkpoints       = 6;

    // Camera inspector
    char camera_json_path[256]    = "";
    bool pending_camera_load      = false;
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
    bool allBounces      = true;   // show points from all bounce depths
    int  bounceFilter    = 0;      // active when allBounces == false

    // --- Pixel picker --------------------------------------------------------
    float pick_r = 0.0f, pick_g = 0.0f, pick_b = 0.0f;
    bool  has_pick = false;

    // --- Splat pass ----------------------------------------------------------
    bool  showSplat  = true;
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
    void drawSplatPanel();
    void drawCapturePanel(const Camera& camera);

    ViewState    state_;
    CaptureState capture_;
    bool showDemoWindow_ = false;
};
