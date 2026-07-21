#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

class HT943 {
public:
   enum class Profile { Standard, Keychain55, GA888 };
   struct Snapshot {
      std::array<uint8_t,256> ram;
      std::array<uint8_t,5> wr;
      uint16_t pc,stack;
      uint64_t instructions;
      int32_t timer_counter,sound_clock_counter;
      double tone_frequency;
      uint8_t acc,tc,pp,pm,ps,pa,sound_note_counter,sound_channel;
      uint16_t flags;
   };
   bool load(const void *data, std::size_t size, Profile profile=Profile::Standard);
   void reset();
   unsigned clock();
   void set_button(unsigned button, bool pressed);
   bool wake_from_deep_sleep();
   const std::array<uint8_t, 256>& vram() const { return ram_; }
   uint16_t pc() const { return pc_; }
   uint64_t instruction_count() const { return instructions_; }
   double tone_frequency() const { return tone_frequency_; }
   bool tone_noise() const { return tone_noise_; }
   unsigned clock_hz() const;
   void save(Snapshot& state) const;
   void restore(const Snapshot& state);

private:
   uint8_t rb(unsigned pair) const;
   void wb(unsigned pair, uint8_t value);
   uint8_t rom(uint32_t address) const;
   void set_pin(char port, unsigned pin, bool pressed);
   void sound_clock(unsigned cycles);
   void sound_channel(unsigned channel);
   void sound_off();
   const uint8_t* sound_rom() const;
   unsigned timer_div() const;
   unsigned sound_freq_div() const;
   const unsigned* sound_speed_div() const;

   std::vector<uint8_t> rom_;
   Profile profile_=Profile::Standard;
   std::array<bool,8> input_buttons_{};
   std::array<uint8_t, 256> ram_{};
   std::array<uint8_t, 5> wr_{};
   uint16_t pc_ = 0, stack_ = 0;
   uint8_t acc_ = 0, tc_ = 0;
   uint8_t pp_ = 15, pm_ = 15, ps_ = 15, pa_ = 0;
   bool cf_ = false, ef_ = false, tf_ = false, ei_ = false;
   bool halt_ = false, reset_pin_ = false, timer_on_ = false;
   int timer_counter_ = 0;
   uint64_t instructions_ = 0;
   int sound_clock_counter_ = 0;
   unsigned sound_note_counter_ = 0, sound_channel_ = 0;
   bool sound_repeat_ = false, sound_on_ = false, tone_noise_ = false;
   double tone_frequency_ = 0.0;
};
