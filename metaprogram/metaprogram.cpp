#include "ev3dev.h"
#include <thread>
#include <iostream>
#include <array>

using namespace ev3dev;

enum Color : int
{
  NoColor = 0,
  Black,
  Blue,
  Green,
  Yellow,
  Red,
  White,
  Brown
};

constexpr const char* ColorStr[] = {
  "NoColor",
  "Black",
  "Blue",
  "Green",
  "Yellow",
  "Red",
  "White",
  "Brown"
};

enum class ProgramReading : int
{
    Stop,
    Continue
};


void StartMovingTape(medium_motor& motor)
{
    motor.set_speed_sp(-70).run_forever();
}

void MoveMainMotor(large_motor& m, std::chrono::milliseconds time, bool forward)
{
    m.set_speed_sp(forward ? 200 : -200);
    m.set_time_sp(static_cast<int>(time.count()));
    m.run_timed();
}

void MoveForward(
    large_motor& leftMotor,
    large_motor& rightMotor)
{
    MoveMainMotor(leftMotor, std::chrono::seconds(3), true);
    MoveMainMotor(rightMotor, std::chrono::seconds(3), true);
}

void TurnLeft(
    large_motor& leftMotor,
    large_motor& rightMotor)
{
    MoveMainMotor(leftMotor, std::chrono::seconds(1), false);
    MoveMainMotor(rightMotor, std::chrono::seconds(1), true);
}

void TurnRight(
    large_motor& leftMotor,
    large_motor& rightMotor)
{
    MoveMainMotor(leftMotor, std::chrono::seconds(1), true);
    MoveMainMotor(rightMotor, std::chrono::seconds(1), false);
}

ProgramReading
ProcessColor(
    std::array<int, 2>& prevColors,
    int colAsInt,
    medium_motor& motor,
    large_motor& leftMotor,
    large_motor& rightMotor)
{
    std::cout << "prevColors[0] = " << ColorStr[prevColors[0]];
    std::cout << ", prevColors[1] = " << ColorStr[prevColors[1]];
    std::cout << ", new color = " << ColorStr[colAsInt] << "\n";

    const bool correctColorChange = prevColors[0] != prevColors[1] && prevColors[1] == colAsInt;

    prevColors[0] = prevColors[1];
    prevColors[1] = colAsInt;

    const auto col = static_cast<Color>(colAsInt);
    if (correctColorChange)
    {
        switch (col)
        {
            case Red:
                motor.stop();
                sound::speak("Red color. Moving forward.", true);
                MoveForward(leftMotor, rightMotor);
                break;

            case Green:
                motor.stop();
                sound::speak("Green color. Turning right", true);
                TurnRight(leftMotor, rightMotor);
                break;

            case Blue:
                motor.stop();
                sound::speak("Blue color. Turning left", true);
                TurnLeft(leftMotor, rightMotor);
                break;

            case Black:
                motor.stop();
                sound::speak("Black color. End of tape.", true);
                return ProgramReading::Stop;

            case White: // Skipping these, it's whitespace for us.
            case NoColor:
                break;

            default:
                motor.stop();
                sound::speak("Unknown color", true);
                break;
        }

        StartMovingTape(motor);
        return ProgramReading::Continue;
    }

    return ProgramReading::Stop;
}

int main() {
    medium_motor progMotor{OUTPUT_A};
    large_motor leftMotor{OUTPUT_B};
    large_motor rightMotor{OUTPUT_C};

    color_sensor colorSensor{INPUT_1};
    colorSensor.set_mode(color_sensor::mode_col_color);

    std::cout << "Starting a loop...\n";
    StartMovingTape(progMotor);
    std::array<int, 2> prevColors = { Black, Black };

    if (colorSensor.color() == Black)
    {
        sound::speak("Feeding the tape.");
        while (colorSensor.color() == Black)
        {
            std::cout << "Feeding the tape...";
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
    }

    auto colorResult = ProcessColor(prevColors, colorSensor.color(), progMotor, leftMotor, rightMotor);
    while (colorResult != ProgramReading::Stop)
    {
        std::cout << "Still reading...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        colorResult = ProcessColor(prevColors, colorSensor.color(), progMotor, leftMotor, rightMotor);
    }
}
