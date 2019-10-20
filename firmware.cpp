#include "application.h"

#include <string>
#include <vector>


#define HASH_LENGTH 2
#define DATA_LENGTH 16
#define HEADER_LENGTH 10
#define CALIB_LENGTH 10
#define SEQ_LENGTH (HEADER_LENGTH + (HASH_LENGTH + DATA_LENGTH) * 2)

uint8_t values[2][2] = {{0, 1}, {1, 0}};

std::vector<uint8_t> get_hash(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> hash = {0, 0};
    for (unsigned long i = 0; i < data.size(); i += 2) {
        hash[0] ^= data[i + 0];
        hash[1] ^= data[i + 1];
    }
    return hash;
}


unsigned long micros_per_sec = 1000000;
unsigned long freq = 120;
unsigned long short_interval_micros = micros_per_sec / freq;

unsigned long infoled_duration = short_interval_micros * SEQ_LENGTH;
unsigned long calib_duration = short_interval_micros * CALIB_LENGTH;


struct PatternStatus {
    uint brightness; // 0-255: 2x the brightness the LED should be
    bool calibrate_active; // whether calibrate pattern should appear
    unsigned int calibrate_index; // index of calibration bits
    bool packet_activate; // whether packet pattern should appear
    unsigned int packet_index; // index of packet bits
};

class ConstPattern;

class Pattern {
protected:
    unsigned long pattern_length{};

public:
    int get_pattern_length() {
        return pattern_length;
    }
    virtual PatternStatus tick(unsigned long time_since_start) = 0;
};

class ConstPattern: public Pattern {
public:
    ConstPattern() {
        pattern_length = infoled_duration;
    }

    PatternStatus tick(unsigned long time_since_start) override {
        return PatternStatus{255, false, 0, true, (time_since_start % pattern_length) / short_interval_micros};
    }
};

class FastBlinkPattern: public Pattern {
    unsigned long calib_start = 0;
    unsigned long calib_end = calib_start + calib_duration;
    unsigned long packet_start = calib_end;
    unsigned long packet_end = packet_start + infoled_duration;
    unsigned long bright_begin = packet_end;
    unsigned long bright_end = pattern_length / 2;
public:
    FastBlinkPattern() {
        pattern_length = micros_per_sec;
    }

    PatternStatus tick(unsigned long time_since_start) override {
        unsigned long time_in_pattern = time_since_start % pattern_length;

        if (time_in_pattern >= calib_start && time_in_pattern < calib_end) {
            unsigned long time_in_calib = time_in_pattern - calib_start;
            return PatternStatus{255, true, time_in_calib / short_interval_micros, false, 0};
        } else if (time_in_pattern >= packet_start && time_in_pattern < packet_end) {
            unsigned long time_in_packet = time_in_pattern - packet_start;
            return PatternStatus{255, false, 0, true, time_in_packet / short_interval_micros};
        } else if (time_in_pattern >= bright_begin && time_in_pattern < bright_end) {
            return PatternStatus{255, false, 0, false, 0};
        } else {
            return PatternStatus{0, false, 0, false, 0};
        }
    }
};

class SlowBlinkPattern: public Pattern {
    unsigned long pattern_length = micros_per_sec * 2;
    unsigned long calib_start = 0;
    unsigned long calib_end = calib_start + calib_duration;
    unsigned long packet_start = calib_end;
    unsigned long packet_end = packet_start + infoled_duration;
    unsigned long bright_begin = 0;
    unsigned long bright_end = pattern_length / 2;
    unsigned long bright_mid = (bright_begin + bright_end) / 2;
public:
    SlowBlinkPattern() {
        pattern_length = micros_per_sec * 2;
    }

    PatternStatus tick(unsigned long time_since_start) override {
        unsigned long time_in_pattern = time_since_start % pattern_length;

        if (time_in_pattern >= bright_begin && time_in_pattern < bright_end) {
            auto time_in_bright = (time_in_pattern - bright_begin) % bright_mid;
            if (time_in_bright >= calib_start && time_in_bright < calib_end) {
                unsigned long time_in_calib = time_in_bright - calib_start;
                return PatternStatus{255, true, time_in_calib / short_interval_micros, false, 0};
            } else if (time_in_bright >= packet_start && time_in_bright < packet_end) {
                unsigned long time_in_packet = time_in_bright - packet_start;
                return PatternStatus{255, false, 0, true, time_in_packet / short_interval_micros};
            } else {
                return PatternStatus{255, false, 0, false, 0};
            }
        } else {
            return PatternStatus{0, false, 0, false, 0};
        }
    }
};

class BreathingPattern: public Pattern {
    unsigned long pattern_length = micros_per_sec * 2;
    unsigned long calib_start = 0;
    unsigned long calib_end = calib_start + calib_duration;
    unsigned long packet_start = calib_end;
    unsigned long packet_end = packet_start + infoled_duration;
    unsigned long half_pattern_length = pattern_length / 2;
    unsigned long bright_begin = half_pattern_length / 2;
    unsigned long bright_end = bright_begin + half_pattern_length;
    unsigned long bright_mid = half_pattern_length / 2;
public:
    BreathingPattern() {
        pattern_length = micros_per_sec * 2;
    }

    PatternStatus tick(unsigned long time_since_start) override {
        unsigned long time_in_pattern = time_since_start % pattern_length;
        uint brightness = 0;
        if (time_in_pattern < half_pattern_length) {
            brightness = 255 * time_in_pattern / half_pattern_length / 2;
        } else {
            brightness = 255 * (pattern_length - time_in_pattern) / half_pattern_length / 2;
        }

        auto time_in_bright = (time_in_pattern - bright_begin) % bright_mid;
        if (time_in_bright >= calib_start && time_in_bright < calib_end) {
            unsigned long time_in_calib = time_in_bright - calib_start;
            return PatternStatus{brightness, true, time_in_calib / short_interval_micros, false, 0};
        } else if (time_in_bright >= packet_start && time_in_bright < packet_end) {
            unsigned long time_in_packet = time_in_bright - packet_start;
            return PatternStatus{brightness, false, 0, true, time_in_packet / short_interval_micros};
        } else {
            return PatternStatus{brightness, false, 0, false, 0};
        }
    }
};


class InfoLedPacket {

    static constexpr uint8_t preamble[HEADER_LENGTH] = {0, 1, 1, 0, 1, 1, 0, 0, 1, 0};
    std::vector<uint8_t> packet_data;

    static std::vector<uint8_t> get_packet(const std::vector<uint8_t>& data) {
        std::vector<uint8_t> packet;

        std::vector<uint8_t> preamble_bits(preamble, preamble + HEADER_LENGTH);
        std::vector<uint8_t> hash = get_hash(data);
        std::vector<uint8_t> hash_bits;
        hash_bits.reserve(hash.size() * 2);
        std::vector<uint8_t> data_bits;
        data_bits.reserve(data.size() * 2);

        for (uint8_t bit : hash) {
            hash_bits.push_back(values[bit][0]);
            hash_bits.push_back(values[bit][1]);
        }

        for (uint8_t bit : data) {
            data_bits.push_back(values[bit][0]);
            data_bits.push_back(values[bit][1]);
        }

        //packet = preamble + hash + data
        packet.insert(packet.end(), preamble_bits.begin(), preamble_bits.end());
        packet.insert(packet.end(), hash_bits.begin(), hash_bits.end());
        packet.insert(packet.end(), data_bits.begin(), data_bits.end());

        return packet;
    }

    static constexpr uint8_t default_packet_data[DATA_LENGTH] = {0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1};

public:
    explicit InfoLedPacket(const uint8_t data[DATA_LENGTH] = default_packet_data) {
        packet_data = get_packet(std::vector<uint8_t>(data, data + DATA_LENGTH));
    }

    static InfoLedPacket get_default_packet() {
        return InfoLedPacket(default_packet_data);
    }

    uint8_t getData(unsigned long index) {
        return packet_data[index];
    }
};

constexpr uint8_t InfoLedPacket::default_packet_data[];
constexpr uint8_t InfoLedPacket::preamble[];


class InfoLed {
private:
    std::vector<uint8_t> pins;
    unsigned long start_timestamp;
    bool enabled = true;

    void activatePin() {
        for (auto& pin : pins) {
            pinMode(pin, OUTPUT);
        }
    }

public:
    InfoLedPacket packet;
    Pattern* pattern;
    bool no_info = false;

    void changePin(const std::vector<uint8_t> &pins) {
        clear();
        this->pins = pins;
        activatePin();
    }

    void clear() {
        for (auto& pin : pins) {
            pinMode(pin, OUTPUT);
            analogWrite(pin, 0, 65536);
        }
    }

    void disable() {
        clear();
        enabled = false;
    }

    void enable() {
        enabled = true;
        start_timestamp = micros();
    }

    explicit InfoLed(const std::vector<uint8_t> &pins, Pattern* pattern = new ConstPattern(), const InfoLedPacket &packet = InfoLedPacket::get_default_packet()) {
        changePin(pins);
        this->pattern = pattern;
        this->packet = packet;
        this->start_timestamp = micros();
    }

    void tick() {
        if (!enabled) return;
        unsigned long time_since_start = micros() - this->start_timestamp;
        auto patternStatus = this->pattern->tick(time_since_start);
        if (patternStatus.calibrate_active) {
            writeLed(patternStatus.calibrate_index % 2, patternStatus.brightness);
        } else if (patternStatus.packet_activate) {
            writeLed(packet.getData(patternStatus.packet_index), patternStatus.brightness);
        } else {
            writeLed(0, patternStatus.brightness, true);
        }
    }

    void writeLed(uint value, uint brightness, bool stable = false) {
        auto divider = 3;
        for (auto& pin : pins) {
            pinMode(pin, OUTPUT);
            if (stable || no_info) {
                analogWrite(pin, brightness / 2 / divider +  (divider - 1) * brightness / divider, 65536);
            } else {
                analogWrite(pin, brightness * value / divider +  (divider - 1) * brightness / divider, 65536);
            }
        }
    }
};

InfoLed* info_led;

void setup() {
    RGBClass::control(true);
    RGBClass::color(0, 0, 0);
    Serial.begin(115200);
    Serial.setTimeout(10);
    info_led = new InfoLed({D0, D1, D2}, new ConstPattern());
}

enum Command: char {
    Color = 'c',
    Info = 'i',
    Start = 'b',
    Stop = 'e',
    Pattern = 'p'
};

const size_t SWITCH_TIME = 5000000;
const std::vector<std::vector<uint8_t>> color_list = {std::vector<uint8_t>{D0}, std::vector<uint8_t>{D1}, std::vector<uint8_t>{D2}, std::vector<uint8_t>{D0, D1, D2}};
auto const color_size = color_list.size();
auto const pattern_size = 4;
auto const total_size = color_size * pattern_size;
size_t color_id = 0;
size_t pattern_id = 0;

void loop() {
    auto iter = micros() / SWITCH_TIME;
    auto new_color_id = iter % color_size;
    auto new_pattern_id = iter / color_size % pattern_size;

    if ((new_color_id != color_id) || (new_pattern_id != pattern_id)) {
        Serial.print(new_color_id);
        Serial.print(',');
        Serial.println(new_pattern_id);
        color_id = new_color_id;
        pattern_id = new_pattern_id;
        delete(info_led->pattern);
        if (pattern_id == 0) {
            info_led->pattern = new ConstPattern();
        } else if (pattern_id == 1) {
            info_led->pattern = new FastBlinkPattern();
        } else if (pattern_id == 2) {
            info_led->pattern = new SlowBlinkPattern();
        } else if (pattern_id == 3) {
            info_led->pattern = new BreathingPattern();
        }
        info_led->changePin(color_list[color_id]);
    }
    info_led->tick();
}



