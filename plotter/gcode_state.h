#ifndef EV3PLOTTER_GCODE_STATE_H_DEFINED
#define EV3PLOTTER_GCODE_STATE_H_DEFINED

namespace ev3plotter {
    struct GCodeState {
        bool relative_moves{false};
        bool use_mm{true};

        // TODO: fix the scale!
        static constexpr const double c_stepsToMm[3]{0.198,0.188, 0.198};
        static constexpr const double c_stepsToInches[3]{0.0077814, 0.0073884, 0.0077814};
    };
}

#endif // EV3PLOTTER_GCODE_STATE_H_DEFINED
