#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

class KS56CX2X {
public:
   enum class Profile { GA878, MiconKC32 };
   struct Snapshot {
      std::array<uint8_t,4096> ram;
      uint32_t pc;
      uint64_t instructions;
      int64_t basic_timer,t0_timer,watch_timer;
      uint32_t cpu_clock_div;
      uint16_t sp;
      uint8_t cy,mbe,mbs,rbe,rbs,sbs,ist,sk,ime,ips,pcc_mode,pcc_clock;
      uint8_t tmod2h,wdtm,bt,btm,scc,wml,wmh,tm0l,tm0h,t0,tmod0;
      uint8_t im0,im1,im2,inta,intc,inte,intf,intg,inth;
      uint8_t ports[8][2],port_latch[6],poga,pogb,pm3,pm6,sound_level;
   };
   bool load(const void*,std::size_t,Profile profile=Profile::GA878);
   void reset();
   unsigned clock();
   void set_button(unsigned,bool);
   bool wake_from_deep_sleep();
   const uint8_t* vram()const{return ram_.data()+0x100;}
   uint16_t pc()const{return uint16_t(pc_&0x1fff);}
   uint64_t instruction_count()const{return instructions_;}
   int sound_level()const{return sound_level_;}
   void save(Snapshot&)const;
   void restore(const Snapshot&);
private:
   enum {XA=0,XAE=1,HL=2,HLE=3,DE=4,DEE=5,BC=6,BCE=7};
   enum {A=0,X=1,L=2,H=3,E=4,D=5,C=6,B=7};
   uint8_t rom(unsigned)const;uint16_t word(unsigned)const;uint32_t bytes(unsigned,unsigned)const;unsigned opcode_size(uint8_t)const;
   uint8_t get_io(unsigned);void set_io(unsigned,uint8_t);
   uint8_t get_mem(unsigned);void set_mem(unsigned,uint8_t);uint8_t get_ahl();uint8_t get_ahl_byte();
   void set_ahl(uint8_t);void set_ahl_byte(uint8_t);uint8_t get_hmem(uint32_t);void set_hmem(uint32_t,uint8_t);
   uint8_t get_pmeml(uint32_t);void set_pmeml(uint32_t,uint8_t);uint8_t get_fmem(uint32_t);void set_fmem(uint32_t,uint8_t);
   uint8_t get_reg(unsigned)const;void set_reg(unsigned,uint8_t);uint8_t get_rp(unsigned)const;void set_rp(unsigned,uint8_t);
   void stack_push(uint8_t);void stack_push_byte(uint8_t);uint8_t stack_pop();uint8_t stack_pop_byte();unsigned skip_next();
   unsigned execute(uint8_t,uint32_t);unsigned exec99(uint32_t);unsigned execaa(uint32_t);unsigned exec_bit_group(uint8_t,uint32_t);
   void go_vector(unsigned);void interrupt(unsigned);unsigned interrupt_vector()const;void process_timers(unsigned);void update_clock_div();
   void restore_pc();void port_input(unsigned,unsigned,bool);

   std::vector<uint8_t> rom_;Profile profile_=Profile::GA878;std::array<uint8_t,4096> ram_{};uint32_t pc_=0;uint16_t sp_=0;
   uint8_t cy_=0,mbe_=0,mbs_=0,rbe_=0,rbs_=0,sbs_=0,ist_=0,sk_=0,ime_=0,ips_=0,pcc_mode_=0,pcc_clock_=0;
   uint8_t tmod2h_=0xff,wdtm_=0,bt_=0,btm_=0,scc_=0,wml_=0,wmh_=0,tm0l_=0,tm0h_=0,t0_=0,tmod0_=0xff;
   uint8_t im0_=0,im1_=0,im2_=0,inta_=0,intc_=0,inte_=0,intf_=0,intg_=0,inth_=0;
   std::array<std::array<uint8_t,2>,8> ports_{}; // 0,1,2,3,5,6,8,9
   std::array<bool,8> input_buttons_{};
   std::array<uint8_t,6> port_latch_{}; // 2,3,5,6,8,9
   uint8_t poga_=0,pogb_=0,pm3_=0,pm6_=0;int sound_level_=0;
   int64_t basic_timer_=0,t0_timer_=0,watch_timer_=0;uint32_t cpu_clock_div_=64;uint64_t instructions_=0;
};
