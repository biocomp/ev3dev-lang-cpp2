#ifndef EV3PLOTTER_MESSAGEQUEUE_H
#define EV3PLOTTER_MESSAGEQUEUE_H


namespace ev3plotter {
    class message_queue {
    public:
        message_queue(const char *name);

    private:
        class impl;

        std::unique_ptr<impl> impl_;
    };
} // namespace ev3plotter

#endif // EV3PLOTTER_MESSAGEQUEUE_H