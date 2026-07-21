#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

class EM73000 {
public:
   struct Snapshot {
      std::array<uint8_t,384> ram;
      uint16_t pc,dp,timer_a,timer_b;
      uint64_t instructions;
      int64_t timer_a_counter,timer_b_counter,time_base_counter;
      double tone_frequency;
      uint32_t ram_bank,sound_basic_div,sound_freq_div,sound_mode;
      uint8_t acc,sp,hl,il,imask;
      uint8_t ports[15];
      uint16_t flags;
   };
   bool load(const void *data, std::size_t size);
   void reset();
   unsigned clock();
   void set_button(unsigned button, bool pressed);
   bool wake_from_deep_sleep();
   const std::array<uint8_t,384>& vram() const { return ram_; }
   uint16_t pc() const { return pc_&0xfff; }
   uint64_t instruction_count() const { return instructions_; }
   double tone_frequency() const { return tone_frequency_; }
   bool tone_noise() const { return tone_noise_; }
   void save(Snapshot& state) const;
   void restore(const Snapshot& state);

private:
   uint8_t rom(unsigned address) const;
   uint16_t word(unsigned address) const;
   uint8_t get_io(unsigned port) const;
   void set_io(unsigned port,uint8_t value);
   void set_pin(unsigned port,unsigned pin,bool pressed);
   void process_timers(unsigned cycles);
   void interrupt();
   void update_sound();

   std::vector<uint8_t> rom_;
   std::array<uint8_t,384> ram_{};
   uint16_t pc_=0,dp_=0,timer_a_=0,timer_b_=0;
   uint8_t acc_=0,sp_=0,hl_=0,il_=0,imask_=0;
   bool ei_=false,cf_=false,zf_=false,sf_=true;
   uint8_t p0_=15,p4_=0,p5_=0,p6_=0,p7_=15,p8_=15,p9_=0,p16_=0;
   uint8_t p23_=15,p24_=15,p25_=0,p27_=0,p28_=0,p29_=0,p30_=0;
   int64_t timer_a_counter_=0,timer_b_counter_=0,time_base_counter_=0;
   unsigned ram_bank_=0;
   unsigned sound_basic_div_=0,sound_freq_div_=0,sound_mode_=0;
   double tone_frequency_=0.0;
   bool tone_noise_=false;
   uint64_t instructions_=0;
};
