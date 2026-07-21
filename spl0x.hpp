#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

class SPL0X {
public:
   struct AudioChannel { double frequency=0,amplitude=0; bool noise=false; };
   struct Snapshot {
      std::array<uint8_t,144> ram;
      std::array<uint8_t,48> lcd;
      int64_t t2_counter,t256_counter;
      uint64_t cycle_counter,instructions;
      uint16_t pc;
      uint8_t sp,a,x;
      uint8_t rom_bank,pdir,platch,input_low,input_high;
      uint8_t io_ctrl,int_cfg,ireq,sys_ctrl,prescaler;
      uint8_t speech_ctrl,speech_data,tonea1,tonea2,toneb1,toneb2,noise1,noise2;
      uint16_t flags;
   };
   enum class Variant { SPL02, SPL03 };
   explicit SPL0X(Variant variant):variant_(variant){}
   bool load(const void*,std::size_t);
   void reset();
   unsigned clock();
   void set_button(unsigned,bool);
   bool wake_from_deep_sleep();
   const std::array<uint8_t,48>& vram()const{return lcd_;}
   uint16_t pc()const{return pc_;}
   uint64_t instruction_count()const{return instructions_;}
   const std::array<AudioChannel,4>& audio_channels()const{return audio_;}
   void save(Snapshot&)const;
   void restore(const Snapshot&);
private:
   uint8_t rom(unsigned)const;uint16_t rom_word_lsb(unsigned)const;unsigned mapped_pc()const;
   uint8_t read(unsigned);void write(unsigned,uint8_t);uint8_t port_read()const;void set_io(unsigned,uint8_t);uint8_t get_io(unsigned);
   void go_vector(unsigned);void timers(unsigned);void nmi();void irq();
   void update_audio();
   uint8_t ps()const;void set_ps(uint8_t);void adc(uint8_t);void sbc(uint8_t);
   Variant variant_;std::vector<uint8_t> rom_;std::array<uint8_t,144> ram_{};std::array<uint8_t,48> lcd_{};
   uint16_t pc_=0;uint8_t sp_=0,a_=0,x_=0;bool nf_=0,vf_=0,df_=0,bf_=0,if_=1,zf_=0,cf_=0;
   bool cpu_on_=1,rosc_on_=1,clock32_on_=1;uint8_t rom_bank_=0,pdir_=0,platch_=0,input_low_=0,input_high_=0;
   uint8_t io_ctrl_=0,int_cfg_=0,ireq_=0,sys_ctrl_=0,prescaler_=1,speech_ctrl_=0,speech_data_=0;
   uint8_t tonea1_=0,tonea2_=0,toneb1_=0,toneb2_=0,noise1_=0,noise2_=0;
   int64_t t2_counter_=0,t256_counter_=0;uint64_t cycle_counter_=0,instructions_=0;
   std::array<AudioChannel,4> audio_{};
};
