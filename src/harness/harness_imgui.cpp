#include "harness_imgui.h"

#include "imgui.h"

#include "globvars.h"

#include <vector>

#define MAX_HISTORY 10000
static int history_size = 1000;

template <typename T>
class Logger {
public:
    Logger(const char *text, T* pointer, float scale_min = FLT_MAX, float scale_max = FLT_MAX)
    : m_pointer(pointer)
    , m_text(text)
    , m_scale_min(scale_min)
    , m_scale_max(scale_max) {
        memset(m_data, 0, sizeof(m_data));
    }
    void plot_lines() {
        log(*m_pointer);
        ImGui::PlotLines(m_text, imgui_plot, this, history_size, 0, NULL, m_scale_min, m_scale_max);
    }
private:
    void log(T d) {
        while (m_index >= MAX_HISTORY) {
            m_index -= MAX_HISTORY;
        }
        m_data[m_index] = (float)d;
        m_index++;
    }
    static float imgui_plot(void *data, int index) {
        Logger<T> &self = *(Logger<T> *)data;
        return self.m_data[((self.m_index - history_size + index) + MAX_HISTORY) % MAX_HISTORY];
    }
    size_t m_index{0};
    float m_data[MAX_HISTORY];
    T *m_pointer = NULL;
    const char *m_text;
    float m_scale_min;
    float m_scale_max;
};

#define CAR_LOGGER(member) { #member, &(gProgram_state.current_car. member) }
#define CAR_LOGGER_MIN_MAX(member, min, max) { #member, &(gProgram_state.current_car. member), min, max }

static Logger<float> float_loggers[] = {
    CAR_LOGGER(speed),
    CAR_LOGGER(sk[0]),
    CAR_LOGGER(sk[1]),
    CAR_LOGGER(initial_brake),
    CAR_LOGGER(brake_increase),
    CAR_LOGGER(freduction),
    CAR_LOGGER(acc_force),
    CAR_LOGGER(torque),
    CAR_LOGGER(brake_force),
};

static Logger<int> int_loggers[] = {
};


#define FLOAT_CAR_SLIDER(member) { &(gProgram_state.current_car. member), #member, 0.f, 100.f }
#define FLOAT_CAR_SLIDER_MINMAX(member, min, max) { &(gProgram_state.current_car. member), #member, min, max }

static struct {
    float* pointer;
    const char* text;
    float min;
    float max;
} FLOAT_CAR_SLIDERs[] = {
    { &gPinball_factor, "gPinball_factor", 0.f, 100.f },
    { &gOpponent_speed_factor, "gOpponent_speed_factor", 0.f, 100.f },
    FLOAT_CAR_SLIDER(proxy_ray_distance),
    FLOAT_CAR_SLIDER(bounce_rate),
    FLOAT_CAR_SLIDER(bounce_amount),
    FLOAT_CAR_SLIDER(collision_mass_multiplier),
    FLOAT_CAR_SLIDER(damage_multiplier),
    FLOAT_CAR_SLIDER(grip_multiplier),
    FLOAT_CAR_SLIDER(engine_power_multiplier),
};


#define INT_CAR_SLIDER(member) { &(gProgram_state.current_car. member), #member, 0, 100 }
#define INT_CAR_SLIDER_MINMAX(member, min, max) { &(gProgram_state.current_car. member), #member, min, max }

static struct {
    int* pointer;
    const char* text;
    int min;
    int max;
} INT_CAR_SLIDERs[] = {
    INT_CAR_SLIDER_MINMAX(damage_units[0].damage_level, 0, 99),
    INT_CAR_SLIDER_MINMAX(damage_units[1].damage_level, 0, 99),
    INT_CAR_SLIDER_MINMAX(damage_units[2].damage_level, 0, 99),
    INT_CAR_SLIDER_MINMAX(damage_units[3].damage_level, 0, 99),
    INT_CAR_SLIDER_MINMAX(damage_units[4].damage_level, 0, 99),
    INT_CAR_SLIDER_MINMAX(damage_units[5].damage_level, 0, 99),
    INT_CAR_SLIDER_MINMAX(damage_units[6].damage_level, 0, 99),
    INT_CAR_SLIDER_MINMAX(damage_units[7].damage_level, 0, 99),
    INT_CAR_SLIDER_MINMAX(damage_units[8].damage_level, 0, 99),
    INT_CAR_SLIDER_MINMAX(damage_units[9].damage_level, 0, 99),
    INT_CAR_SLIDER_MINMAX(damage_units[10].damage_level, 0, 99),
    INT_CAR_SLIDER_MINMAX(damage_units[11].damage_level, 0, 99),
};


void dethrace_imgui_init() {

}


void dethrace_imgui() {
    ImGui::ShowDemoWindow();
    static bool open = true;
    if (!ImGui::Begin("gProgram_state.current_car", &open, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }
    ImGui::Checkbox("gShow_peds_on_map", (bool*)&gShow_peds_on_map);
    ImGui::Checkbox("gShow_opponents", (bool*)&gShow_opponents);
    ImGui::SliderInt("History size", &history_size, 100, 10000);
    history_size = MAX(history_size, 10000);
    ImGui::SliderInt3("power_up_levels", gProgram_state.current_car.power_up_levels, 0, 5);
    for (size_t i = 0; i < IM_ARRAYSIZE(INT_CAR_SLIDERs); i++) {
        ImGui::SliderInt(INT_CAR_SLIDERs[i].text, INT_CAR_SLIDERs[i].pointer, INT_CAR_SLIDERs[i].min, INT_CAR_SLIDERs[i].max);
    }
    for (size_t i = 0; i < IM_ARRAYSIZE(FLOAT_CAR_SLIDERs); i++) {
        ImGui::SliderFloat(FLOAT_CAR_SLIDERs[i].text, FLOAT_CAR_SLIDERs[i].pointer, FLOAT_CAR_SLIDERs[i].min, FLOAT_CAR_SLIDERs[i].max);
    }
    for (size_t i = 0; i < IM_ARRAYSIZE(float_loggers); i++) {
        float_loggers[i].plot_lines();
    }
    for (size_t i = 0; i < IM_ARRAYSIZE(int_loggers); i++) {
        int_loggers[i].plot_lines();
    }
    ImGui::End();
}
