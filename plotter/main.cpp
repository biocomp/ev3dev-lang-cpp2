/*
 * Copyright (c) 2014 - Franz Detro
 *
 * Some real world test program for motor control
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "menu.h"
#include "display.h"

using namespace ev3plotter;
#include <thread>
#include <chrono>
#include <string>
#include <numeric>
#include <iostream>

struct Motor
{
    Motor(std::string nameSrc, const char* port) : name{std::move(nameSrc)}, motor{port}
    {}

    std::string name;
    ev3dev::large_motor motor;
};

// template <typename T>
// struct Array
// {
//     template <std::size_t N>
//     constexpr Array(T (&arr)[N]) noexcept : data{arr}, size{N}
//     {}

//     T* data;
//     std::size_t size;
// };

template <typename... TMotors>
void drive(int speed, int distance, TMotors&&... motors)
{
    std::string names;
    auto addName{[&names](const auto& motor){ names += " " + motor.name; }};

    {
        std::initializer_list<int> dummy{(addName(motors), 0)...};
        (void)dummy;
    }

    std::cout << "Driving " << names << ", to " << distance << "at speed " << speed << "\n";

    auto startDriving{[distance, speed](Motor& motor){
        motor.motor.set_position_sp(distance).set_speed_sp(speed).run_to_rel_pos();
    }};

    {
        std::initializer_list<int> dummy{(startDriving(motors), 0)...};
        (void)dummy;
    }

    auto running{[](const Motor& motor){
        return motor.motor.state().count("running") != 0;
    }};

    auto allStopped{[&](){
        std::initializer_list<int> runningCounts{running(motors)...};
        return std::accumulate(runningCounts.begin(), runningCounts.end(), 0, [](auto a, auto b){ return a + b; }) == 0;
    }};

    while (!allStopped())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "Done driving " << names << "!\n";
}

void wait_for_back_press() {
    bool backPressed{false};
    while (!backPressed)
    {
        backPressed = ev3dev::button::back.pressed ();
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }
}

bool verify_device(const ev3dev::lcd& display) {
    if (display.bits_per_pixel() != 1)
    {
        //std::cout << "########\n";
        //std::cout << "display does not have 1 bits per pixel! It has " << display.bits_per_pixel() << ". Failing..." << std::endl;
        //wait_for_back_press();
        //return false;
    }

    return true;
}


void draw_something(ev3dev::lcd& lcd) {
    auto* buffer = lcd.frame_buffer();

    display d{buffer, static_cast<int>(lcd.resolution_x()), static_cast<int>(lcd.resolution_y())};

    // std::this_thread::sleep_for(std::chrono::seconds{2});
    // display.fill(0xff);
    // std::this_thread::sleep_for(std::chrono::seconds{2});


    bool backPressed{false};
    int x = 0;
    while (!backPressed)
    {
        if ((x + 40) >= d.width) {
            x = 0;
        }

        d.fill(false);
        rectangle(d, {{x, 10}, {x + 40, 10 + 40}}, true);
        print_text(d, {x + 1, 11}, "ABAB", false);
        print_text(d, {x + 41, 11}, "ABBAA", true);

        backPressed = ev3dev::button::back.pressed ();
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
        //++x;
    }
}

int main()
{
    ev3dev::lcd display{};

    if (!verify_device(display))
    {
        return -1;
    }

    draw_something(display);

    wait_for_back_press();

    //Motor x{"X", ev3dev::OUTPUT_A};
    //Motor y{"Y", ev3dev::OUTPUT_B};

    //x.motor.

    //drive(100, 500, x, y);
    //drive(100, -500, x, y);

    return 0;
}
