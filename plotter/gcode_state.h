#ifndef EV3PLOTTER_GCODE_STATE_H_DEFINED
#define EV3PLOTTER_GCODE_STATE_H_DEFINED

namespace ev3plotter {
    struct GCodeState {
        bool relative_moves{false};
        bool use_mm{true};

        // TODO: fix the scale!
        static constexpr const double c_stepsToMm[3]{0.1,0.1,0.1};
        static constexpr const double c_stepsToInches[3]{0.00397, 0.00397, 0.00397};
    };
}

#endif // EV3PLOTTER_GCODE_STATE_H_DEFINED
