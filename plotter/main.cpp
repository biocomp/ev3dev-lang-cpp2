#include <widgets.h>
#include <display.h>
#include <driver.h>
#include <ev3dev.h>

#include <thread>
#include <chrono>
#include <string>
#include <numeric>
#include <iostream>
#include <functional>
#include <string_view>
#include <type_traits>
#include <atomic>
#include <mutex>


#include <mqueue/message_queue.h>

// #include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>



using namespace ev3plotter;

// class message_queue{
// public:
//     void push(std::string msg) {
//         std::lock_guard lock{lock_};
//         queue_.push_back(msg);
//     }

//     template <typename THandler>
//     void pop_all(THandler&& h) {
//         std::lock_guard lock{lock_};
//         for (auto&& m : queue_) {
//             h(std::move(m));
//         }
//         queue_.clear();
//     }

// private:
//     std::vector<std::string> queue_;
//     std::mutex lock_;
// };

// std::vector<std::string> messages{};

int main()
{
    std::atomic_bool exit{false};

    message_queue to_main{"/ev3plotter_input"};
    message_queue to_console{"/ev3plotter_output"};

    std::thread main{
        [&]() {
            std::cout << "Enter gcode commands after homing is done..." << std::endl;

            struct termios oldSettings, newSettings;

            while (!exit) {
                tcgetattr( fileno( stdin ), &oldSettings );
                newSettings = oldSettings;
                newSettings.c_lflag &= static_cast<tcflag_t>(~ICANON & ~ECHO);
                tcsetattr( fileno( stdin ), TCSANOW, &newSettings );


                fd_set set;
                struct timeval tv;

                tv.tv_sec = 6;
                tv.tv_usec = 0;

                FD_ZERO( &set );
                FD_SET( fileno( stdin ), &set );

                int res = select( fileno( stdin )+1, &set, NULL, NULL, &tv );

                if( res > 0 )
                {
                    char c;
                    printf( "Input available\n" );
                    (void)!read( fileno( stdin ), &c, 1 );
                    printf("Read: %c\n", c);
                }
                else if( res < 0 )
                {
                    //perror( "select error" );
                    break;
                }
                else
                {
                    //printf( "Select timeout\n" );
                }
            }

        }
    };

    ev3dev::lcd display{};

    state s;

    const StaticMenu *main_menu_ptr{};

    StaticMenu exit_menu{
        "Exit?",
        {
            {"yes", [&exit] { exit = true; }},
            {"no", [&]() { s.set_widget(main_menu_ptr->make()); }}
        }
    };

    Message message{"Please connect motors as follows:",
                    "Output A: tool head motor\n"
                    "Output B: X motor\n"
                    "Output C: Y motor\n",
                    "Close", [&]() { s.set_widget(main_menu_ptr->make()); }};

    ev3plotter::display d{display.frame_buffer(), static_cast<int>(display.resolution_x()), static_cast<int>(display.resolution_y())};

    const IWidget *show_homing_limits_return_widget{nullptr};
    Message show_homing_results{
        "Homing results:",
        "Homing not done!",
        "Exit",
        [&] { s.set_widget(show_homing_limits_return_widget->make()); }};

    const IWidget *utilities_menu_ptr{nullptr};
    const auto if_homed{[&](auto do_when_homed) {
        if (s.homed_)
        {
            do_when_homed();
        }
        else
        {
            show_homing_limits_return_widget = utilities_menu_ptr;
            s.set_widget(show_homing_results.make());
        }
    }};

    StaticMenu utilities_menu{
        "Utilities", {{"< Back", [&] { show_homing_limits_return_widget = main_menu_ptr; s.set_widget(main_menu_ptr->make()); }}, {"x = 0", [&] { if_homed([&] { commands::go(s, pos::x(*s.homed_, normalized_pos{0}), {}, {}); }); }}, {"x+10", [&] { if_homed([&] { commands::go(s, pos::x(*s.homed_, pos::read_x(s) + normalized_pos{10}), {}, {}); }); }}, {"x+100", [&] { if_homed([&] { commands::go(s, pos::x(*s.homed_, pos::read_x(s) + normalized_pos{100}), {}, {}); }); }}, {"x-10", [&] { if_homed([&] { commands::go(s, pos::x(*s.homed_, pos::read_x(s) + normalized_pos{-10}), {}, {}); }); }}, {"x-100", [&] { if_homed([&] { commands::go(s, pos::x(*s.homed_, pos::read_x(s) + normalized_pos{-100}), {}, {}); }); }}, {"y = 0", [&] { if_homed([&] { commands::go(s, {}, pos::y(*s.homed_, normalized_pos{0}), {}); }); }}, {"y+10", [&] { if_homed([&] { commands::go(s, pos::y(*s.homed_, pos::read_y(s) + normalized_pos{10}), {}, {}); }); }}, {"y+100", [&] { if_homed([&] { commands::go(s, pos::y(*s.homed_, pos::read_y(s) + normalized_pos{100}), {}, {}); }); }}, {"y-10", [&] { if_homed([&] { commands::go(s, pos::y(*s.homed_, pos::read_y(s) + normalized_pos{-10}), {}, {}); }); }}, {"y-100", [&] { if_homed([&] { commands::go(s, pos::y(*s.homed_, pos::read_y(s) + normalized_pos{-100}), {}, {}); }); }}, {"Tool up", [&] { if_homed([&] { commands::go(s, {}, {}, pos::z(*s.homed_, normalized_pos{0})); }); }}, {"Tool down", [&] { if_homed([&] { commands::go(s, {}, {}, pos::z(*s.homed_, normalized_pos{0}) + raw_pos{pos::z_travel(*s.homed_).get()}); }); }}}};

    utilities_menu_ptr = &utilities_menu;

    StaticMenu main_menu{
        "Main menu", {{"home", [&]() {
                            s.homed_ = commands::home(s, d, *main_menu_ptr);
                            show_homing_results.update_text(print_homing_results(*s.homed_));
                        }},
                        {"display required connections", [&]() { s.set_widget(message.make()); }},
                        {"show homing results", [&]() { s.set_widget(show_homing_results.make()); }},
                        {"utilities", [&] { s.set_widget(utilities_menu.make()); }, has_more{true}},
                        {"exit", [&]() { s.set_widget(exit_menu.make()); }}}};

    main_menu_ptr = &main_menu;
    show_homing_limits_return_widget = &main_menu;

    s.set_widget(main_menu.make());

    auto prev_draw_time{std::chrono::steady_clock::now()};
    while (!exit)
    {
        s.handle_events();

        const auto now = std::chrono::steady_clock::now();
        if (s.draw(d, now - prev_draw_time > std::chrono::milliseconds{200}))
        {
            prev_draw_time = now;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }



    main.join();

    std::cout << "Joined!..." << std::endl;


/*
struct termios oldSettings, newSettings;




    std::atomic_bool done{false};
    std::thread main{[&] {
        printf("Enter something!!!!\n");

        for (int i = 0; i != 5; ++i)
        {
            if (done)
            {
                printf("DOOONE - someone set it!!!!...");
                break;
            }

            std::this_thread::sleep_for(std::chrono::seconds{1});

            printf("Done something.... %i\n", i);
        }

        done = true;
    }};

    printf("Start\n");


    int val;


    while (!done) {
        tcgetattr( fileno( stdin ), &oldSettings );
        newSettings = oldSettings;
        newSettings.c_lflag &= static_cast<tcflag_t>(~ICANON & ~ECHO);
        tcsetattr( fileno( stdin ), TCSANOW, &newSettings );


        fd_set set;
        struct timeval tv;

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        FD_ZERO( &set );
        FD_SET( fileno( stdin ), &set );

        int res = select( fileno( stdin )+1, &set, NULL, NULL, &tv );

        if( res > 0 )
        {
            char c;
            printf( "Input available\n" );
            (void)!read( fileno( stdin ), &c, 1 );
            printf("Read: %c\n", c);
            done = true;
        }
        else if( res < 0 )
        {
            perror( "select error" );
            break;
        }
        else
        {
            printf( "Select timeout\n" );
        }
    }



   printf("VAL is %c\n", val);

    if (main.joinable()) {
        printf("Joinable!");
        main.join();
    } else {
        printf("NOT Joinable :(!");
    }

   printf("Done");*/

   return 0;
}
