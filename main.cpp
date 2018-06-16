#include <gtkmm.h>
#include <gdkmm.h>
#include <iostream>
#include <cstring>
#include <map>
#include <unistd.h>

#include <alsa/asoundlib.h>

#include "wayfire-shell-client-protocol.h"
#include <wayland-client.h>
#include <gdk/gdkwayland.h>

namespace simple_audio
{
    static const char *card = "default";
    static const char *selem_name = "Master";

    snd_mixer_t *handle;
    snd_mixer_selem_id_t *sid;
    snd_mixer_elem_t *elem;

    long min, max;

    void setup_audio()
    {
        snd_mixer_open(&handle, 0);
        snd_mixer_attach(handle, card);
        snd_mixer_selem_register(handle, NULL, NULL);
        snd_mixer_load(handle);

        snd_mixer_selem_id_alloca(&sid);
        snd_mixer_selem_id_set_index(sid, 0);
        snd_mixer_selem_id_set_name(sid, selem_name);

        elem = snd_mixer_find_selem(handle, sid);
        snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    }

    void cleanup_audio()
    {
        snd_mixer_close(handle);
    }

    int get_level()
    {
        setup_audio();

        long volume;
        snd_mixer_selem_get_playback_volume(elem, (snd_mixer_selem_channel_id_t)0, &volume);

        auto dta = 100.0 * volume / max;

        cleanup_audio();
        return dta + 0.5;
    }

    void set_level(long level)
    {
        setup_audio();
        snd_mixer_selem_set_playback_volume_all(elem, level * max / 100.0);
        cleanup_audio();
    }
}

class SoundWindow;
struct WayfireDisplay
{
    zwf_shell_manager_v1 *zwf_shell_manager;
    std::map<uint32_t, SoundWindow*> output_window;

    Glib::RefPtr<Gtk::Application> app;

    long current_volume;

    WayfireDisplay(decltype(app) a);
    void init(wl_display *display);

    ~WayfireDisplay();
};

class SoundWindow
{
    zwf_output_v1 *zwf_output;
    zwf_wm_surface_v1 *wm_surface;
    public:

    Gtk::Window window;
    Gtk::Box box;
    Gtk::Image img;
    Gtk::Scale scale;

    WayfireDisplay *display;

    SoundWindow(WayfireDisplay *display,
                struct wl_output *wl_output) : window()
    {
        this->display = display;
        box.set_orientation(Gtk::ORIENTATION_HORIZONTAL);

        scale.set_range(0, 100);
        scale.set_draw_value(false);
        scale.set_margin_top(10);
        scale.set_margin_bottom(10);
        scale.set_size_request(150, 30);
        scale.set_digits(0);

        update_volume(display->current_volume);
        scale.signal_value_changed().connect(sigc::mem_fun(*this, &SoundWindow::on_value_changed));
        scale.signal_scroll_event().connect_notify(sigc::mem_fun(*this, &SoundWindow::on_scroll));

        auto size = Gtk::IconSize::register_new("mybig", 30, 30);
        img.set_from_icon_name("audio-volume-low", size);
        img.set_margin_right(10);

        box.add(img);
        box.add(scale);
        box.set_margin_left(30);
        box.set_margin_right(20);
        auto style = window.get_style_context();
        auto provider = Gtk::CssProvider::create();

        style->add_provider(provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

        provider->load_from_data("window {border-radius: 12px; }");

        window.add(box);
        window.show_all_children();

        window.set_decorated(false);
        window.set_resizable(false);

        window.signal_leave_notify_event().connect_notify(sigc::mem_fun(*this, &SoundWindow::on_leave));
        window.signal_enter_notify_event().connect_notify(sigc::mem_fun(*this, &SoundWindow::on_enter));
        scale.signal_leave_notify_event().connect_notify(sigc::mem_fun(*this, &SoundWindow::on_leave));
        scale.signal_enter_notify_event().connect_notify(sigc::mem_fun(*this, &SoundWindow::on_enter));
        img.signal_leave_notify_event().connect_notify(sigc::mem_fun(*this, &SoundWindow::on_leave));
        img.signal_enter_notify_event().connect_notify(sigc::mem_fun(*this, &SoundWindow::on_enter));

        window.show_all();

        auto gdk_win = window.get_window()->gobj();
        auto surf = gdk_wayland_window_get_wl_surface(gdk_win);

        zwf_output = zwf_shell_manager_v1_get_wf_output(display->zwf_shell_manager, wl_output);
        wm_surface = zwf_output_v1_get_wm_surface(zwf_output, surf, ZWF_OUTPUT_V1_WM_ROLE_PANEL);
        zwf_wm_surface_v1_configure(wm_surface, 100, 100);

        display->app->add_window(window);
    }

    void on_scroll(GdkEventScroll *scroll)
    {
        /* TODO: control volume via scroll */
    }

    void on_value_changed()
    {
        simple_audio::set_level(scale.get_value());
        on_enter(NULL);
        on_leave(NULL);
    }

    void update_volume(long volume)
    {
        scale.set_value(volume);
        on_enter(NULL);
        on_leave(NULL);
    }

    sigc::connection timeout;
    void on_enter(GdkEventCrossing *ev)
    {
        timeout.disconnect();
    }

    void on_leave(GdkEventCrossing *)
    {
        timeout = Glib::signal_timeout().connect(sigc::mem_fun(*this, &SoundWindow::on_timeout), 2000);
    }

    bool on_timeout()
    {
        timeout.disconnect();
        display->app->quit();
        return false;
    }

    ~SoundWindow()
    {
        zwf_wm_surface_v1_destroy(wm_surface);
        zwf_output_v1_destroy(zwf_output);
    }
};

/* wayland protocols */
void registry_add_object(void *data, struct wl_registry *registry, uint32_t name,
        const char *interface, uint32_t version)
{
    auto display = (WayfireDisplay*) data;
    if (strcmp(interface, zwf_shell_manager_v1_interface.name) == 0)
    {
        display->zwf_shell_manager =
            (zwf_shell_manager_v1*) wl_registry_bind(registry, name,
                                                     &zwf_shell_manager_v1_interface,
                                                     std::min(version, 1u));
    }
    else if (strcmp(interface, wl_output_interface.name) == 0)
    {
        auto output = (wl_output*) wl_registry_bind(registry, name, &wl_output_interface,
                                                    std::min(version, 1u));

        display->output_window[name] = new SoundWindow(display, output);
    }
}

void registry_remove_object(void *data, struct wl_registry *registry, uint32_t name)
{
    auto display = (WayfireDisplay*) data;
    if (display->output_window.count(name))
    {
        delete display->output_window[name];
        display->output_window.erase(name);
    }
}

static struct wl_registry_listener registry_listener =
{
    &registry_add_object,
    &registry_remove_object
};

/* wayfire_display implementation */
WayfireDisplay::WayfireDisplay(decltype(app) a)
{
    this->app = a;
}

void WayfireDisplay::init(wl_display *display)
{
    wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, this);
    wl_display_roundtrip(display);
}

WayfireDisplay::~WayfireDisplay()
{
    // TODO: clean up, if we really need */
}

void on_activate(Glib::RefPtr<Gtk::Application> app, WayfireDisplay* display)
{
    display->current_volume = simple_audio::get_level();

    for (auto& sw : display->output_window)
        sw.second->update_volume(display->current_volume);
}

void on_startup(Glib::RefPtr<Gtk::Application> app, WayfireDisplay *display)
{
    auto dm = Gdk::DisplayManager::get();
    auto gdisp = dm->get_default_display()->gobj();

    display->init(gdk_wayland_display_get_wl_display(gdisp));
}

int main(int argc, char *argv[])
{
    /* TODO: try to properly parse args, maybe using gtk */
    if (argc > 2)
    {
        std::string action = argv[1];
        long delta = std::atol(argv[2]);

        if (action == "d" or action == "dec" or action == "decrease")
        {
            delta *= -1;
        }
        else if (action == "i" or action == "inc" or action == "increase")
        {
            delta *= 1;
        }
        else
        {
            std::cerr << "Invalid action" << std::endl;
            delta *= 0;
        }

        long now = simple_audio::get_level();

        now = std::min(100l, std::max(now + delta, 0l));
        simple_audio::set_level(now);

        /* skip the first 2 options */
        argc -= 2;
        argv = &argv[2];
    }

    auto app = Gtk::Application::create(argc, argv, "org.wayfire.SoundPopup",
                                        Gio::APPLICATION_HANDLES_OPEN);
    WayfireDisplay display(app);
    app->signal_activate().connect_notify(sigc::bind(&on_activate, app, &display));
    app->signal_startup().connect_notify(sigc::bind(&on_startup, app, &display));

    app->run();
}
