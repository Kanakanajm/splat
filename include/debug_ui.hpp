#pragma once

#include <cstdint>
#include <vector>

class Camera;
struct GLFWwindow;

struct ViewState {
    // --- Geometry ------------------------------------------------------------
    bool showGeometry = false;
    enum class GeomAov : int { None, Diffuse, Normal, Depth, Backface } geomAov = GeomAov::None;
    std::vector<bool> instanceVisible;  // per-instance; empty = all visible

    // --- Photon Points -------------------------------------------------------
    bool showPoints = true;
    enum class PointAov : int { InstanceId, BsdfKind, BounceDepth,
                               PowerColor, PowerLuminance, PowerNormalized } pointAov = PointAov::InstanceId;
    std::vector<bool> instancePointsVisible;  // per-instance; empty = all visible
    bool allBounces      = true;   // show points from all bounce depths
    int  bounceFilter    = 0;      // active when allBounces == false

    // --- Pixel picker --------------------------------------------------------
    float pick_r = 0.0f, pick_g = 0.0f, pick_b = 0.0f;
    bool  has_pick = false;

    // --- Splat pass ----------------------------------------------------------
    bool  showSplat = false;
    float splatH    = 0.1f;

    // --- Photon Beams --------------------------------------------------------
    bool showBeams = true;
    enum class BeamAov : int { MediumId, T, BounceDepth, Length,
                               BeamPowerStart, BeamTransmittancePreview } beamAov = BeamAov::MediumId;
    std::vector<bool> mediumBeamsVisible;  // per-medium; empty = all visible
    bool allBeamBounces   = true;
    int  beamBounceFilter = 0;
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

    const ViewState& viewState() const { return state_; }

private:
    void drawGeometryPanel();
    void drawPhotonPointPanel(uint32_t max_bounce);
    void drawPhotonBeamPanel(uint32_t max_bounce);
    void drawSplatPanel();

    ViewState state_;
    bool showDemoWindow_ = false;
};
