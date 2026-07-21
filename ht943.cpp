#include "ht943.hpp"
#include "generated_sound_rom.hpp"
#include <cstring>

namespace {
constexpr unsigned lfsr_to_div[128] = {
 0,2,123,3,124,75,117,4,125,101,111,76,118,42,69,5,126,66,63,102,112,86,36,77,119,
 21,95,43,70,25,105,6,127,115,99,67,64,34,19,103,113,17,15,87,37,55,89,78,120,39,
 60,22,96,52,57,44,71,91,30,26,106,47,80,7,1,122,74,116,100,110,41,68,65,62,85,35,
 20,94,24,104,114,98,33,18,16,14,54,88,38,59,51,56,90,29,46,79,121,73,109,40,61,84,
 93,23,97,32,13,53,58,50,28,45,72,108,83,92,31,12,49,27,107,82,11,48,81,10,9,8};
constexpr unsigned speed_div_standard[16] = {7,41,103,31,63,63,109,31,123,119,1,37,17,114,11,11};
constexpr unsigned speed_div_keychain55[16] = {7,41,103,31,63,63,41,31,123,84,1,37,17,10,11,11};
constexpr unsigned speed_div_ga888[16] = {7,41,103,31,63,63,41,31,123,119,1,37,17,114,11,11};
constexpr bool channel_effect[16] = {false,false,false,true,true,true,false,false,false,false,false,true,false,false,false,false};
}

bool HT943::load(const void *data, std::size_t size, Profile profile) {
   if (!data || size != 4096) return false;
   profile_=profile;
   rom_.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
   reset();
   return true;
}

void HT943::reset() {
   ram_.fill(0); wr_.fill(0); pc_ = stack_ = 0; acc_ = tc_ = 0;
   pp_ = pm_ = ps_ = 15; pa_ = 0; cf_ = ef_ = tf_ = ei_ = false;
   halt_ = reset_pin_ = timer_on_ = false; timer_counter_ = 0; instructions_ = 0;
   sound_clock_counter_=0; sound_note_counter_=sound_channel_=0; sound_repeat_=sound_on_=false;
   tone_frequency_=0.0; tone_noise_=false; input_buttons_.fill(false);
}

unsigned HT943::clock_hz() const { return profile_==Profile::Keychain55?512000:1000000; }
unsigned HT943::timer_div() const { return profile_==Profile::Keychain55?8:16; }
unsigned HT943::sound_freq_div() const { return profile_==Profile::Keychain55?32:64; }
const unsigned* HT943::sound_speed_div() const {
   return profile_==Profile::Keychain55?speed_div_keychain55:profile_==Profile::GA888?speed_div_ga888:speed_div_standard;
}
const uint8_t* HT943::sound_rom() const {
   return profile_==Profile::Keychain55?ht943_sound_rom_keychain55:profile_==Profile::GA888?ht943_sound_rom_ga888:ht943_sound_rom_e23;
}

void HT943::save(Snapshot& s) const {
   s.ram=ram_;s.wr=wr_;s.pc=pc_;s.stack=stack_;s.instructions=instructions_;
   s.timer_counter=timer_counter_;s.sound_clock_counter=sound_clock_counter_;s.tone_frequency=tone_frequency_;
   s.acc=acc_;s.tc=tc_;s.pp=pp_;s.pm=pm_;s.ps=ps_;s.pa=pa_;
   s.sound_note_counter=uint8_t(sound_note_counter_);s.sound_channel=uint8_t(sound_channel_);
   s.flags=uint16_t(unsigned(cf_)|(unsigned(ef_)<<1)|(unsigned(tf_)<<2)|(unsigned(ei_)<<3)|
                    (unsigned(halt_)<<4)|(unsigned(reset_pin_)<<5)|(unsigned(timer_on_)<<6)|
                    (unsigned(sound_repeat_)<<7)|(unsigned(sound_on_)<<8)|(unsigned(tone_noise_)<<9));
}
void HT943::restore(const Snapshot& s) {
   ram_=s.ram;wr_=s.wr;pc_=s.pc;stack_=s.stack;instructions_=s.instructions;
   timer_counter_=s.timer_counter;sound_clock_counter_=s.sound_clock_counter;tone_frequency_=s.tone_frequency;
   acc_=s.acc;tc_=s.tc;pp_=s.pp;pm_=s.pm;ps_=s.ps;pa_=s.pa;
   sound_note_counter_=s.sound_note_counter;sound_channel_=s.sound_channel;
   cf_=s.flags&1;ef_=s.flags&2;tf_=s.flags&4;ei_=s.flags&8;halt_=s.flags&16;reset_pin_=s.flags&32;
   timer_on_=s.flags&64;sound_repeat_=s.flags&128;sound_on_=s.flags&256;tone_noise_=s.flags&512;
}

void HT943::sound_off() { sound_on_=false; tone_frequency_=0.0; }
void HT943::sound_channel(unsigned channel) {
   sound_on_=true; sound_note_counter_=0; sound_channel_=channel&15;
}
void HT943::sound_clock(unsigned cycles) {
   if(!sound_on_)return;
   sound_clock_counter_-=int(cycles);
   if(sound_clock_counter_<=0) {
      sound_clock_counter_+=int(lfsr_to_div[sound_speed_div()[sound_channel_]]*sound_freq_div()*16);
      unsigned channel_size=32*((sound_channel_>=12)+1);
      unsigned offset=sound_channel_*32;
      if(sound_channel_>12)offset+=(sound_channel_-12)*32;
      uint8_t note=sound_rom()[offset+sound_note_counter_];
      tone_frequency_=note?double(clock_hz())/sound_freq_div()/lfsr_to_div[note]*2.0:0.0;
      tone_noise_=channel_effect[sound_channel_];
      sound_note_counter_=(sound_note_counter_+1)%channel_size;
      if(sound_note_counter_==0&&!sound_repeat_)sound_off();
   }
}

uint8_t HT943::rom(uint32_t a) const { return rom_.empty() ? 0 : rom_[a % rom_.size()]; }
uint8_t HT943::rb(unsigned p) const { return ram_[(wr_[p + 1] << 4) | wr_[p]]; }
void HT943::wb(unsigned p, uint8_t v) { ram_[(wr_[p + 1] << 4) | wr_[p]] = v & 15; }

void HT943::set_pin(char port, unsigned pin, bool pressed) {
   if(port=='-')return;
   uint8_t *p = port == 'P' ? &pp_ : port == 'S' ? &ps_ : &pm_;
   if (pressed) *p &= uint8_t(~(1u << pin)); else *p |= uint8_t(1u << pin);
   unsigned wake_pin=profile_==Profile::Standard?2:0;
   if (halt_ && port == 'S' && pin == wake_pin && pressed) { ef_ = true; halt_ = false; }
}

void HT943::set_button(unsigned b, bool pressed) {
   // 0 left, 1 right, 2 down, 3 rotate, 4 start, 5 on/off, 6 mute.
   static const char standard_ports[7]={'P','P','P','P','S','S','S'};
   static const uint8_t standard_pins[7]={3,2,1,0,0,2,1};
   static const char keychain_ports[7]={'M','M','M','M','S','-','M'};
   static const uint8_t keychain_pins[7]={3,2,1,0,0,0,2};
   static const char ga888_ports[7]={'M','M','M','M','S','-','-'};
   static const uint8_t ga888_pins[7]={3,2,1,0,0,0,0};
   if(b==7){if(pressed)reset();return;}if(b>=7)return;
   input_buttons_[b]=pressed;
   const char*ports=profile_==Profile::Keychain55?keychain_ports:profile_==Profile::GA888?ga888_ports:standard_ports;
   const uint8_t*pins=profile_==Profile::Keychain55?keychain_pins:profile_==Profile::GA888?ga888_pins:standard_pins;
   if(ports[b]=='-')return;
   bool combined=false;
   for(unsigned i=0;i<7;i++)if(ports[i]==ports[b]&&pins[i]==pins[b])combined|=input_buttons_[i];
   set_pin(ports[b],pins[b],combined);
}

bool HT943::wake_from_deep_sleep() {
   if(!halt_)return false;
   set_pin('S',profile_==Profile::Standard?2:0,true);
   return true;
}

unsigned HT943::clock() {
   if (halt_ || reset_pin_) return 8;
   if (ei_ && stack_ == 0) {
      if (ef_) { ef_ = false; stack_ = uint16_t((cf_ << 12) | (pc_ & 0xfff)); pc_ = uint16_t((pc_ & 0xf000) | 8); }
      else if (tf_) { tf_ = false; stack_ = uint16_t((cf_ << 12) | (pc_ & 0xfff)); pc_ = uint16_t((pc_ & 0xf000) | 4); }
   }
   const uint8_t op = rom(pc_);
   unsigned cycles = 4;
   auto next = [&](){ ++pc_; };
   auto imm = [&](){ return uint8_t(rom(pc_ + 1) & 15); };
   auto add = [&](unsigned v, unsigned carry){ unsigned n = acc_ + v + carry; cf_ = n > 15; acc_ = uint8_t(n & 15); };
   auto sub = [&](unsigned v){ unsigned n = acc_ + ((~v) & 15) + 1; cf_ = n > 15; acc_ = uint8_t(n & 15); };

   if (op >= 0x80) {
      if (op < 0xa0) { uint8_t lo=rom(pc_+1); pc_+=2; if (acc_ & (1u<<((op>>3)&3))) pc_=uint16_t((pc_&0xf800)|((op&7)<<8)|lo); }
      else if (op < 0xe0) {
         uint8_t lo=rom(pc_+1); pc_+=2; bool take=false;
         switch ((op>>3)&7) { case 4: take=wr_[0]!=0; break; case 5: take=wr_[1]!=0; break; case 6: take=acc_==0; break; case 7: take=acc_!=0; break; case 0: take=cf_; break; case 1: take=!cf_; break; case 2: take=tf_; if(take) tf_=false; break; case 3: take=wr_[4]!=0; break; }
         if(take) pc_=uint16_t((pc_&0xf800)|((op&7)<<8)|lo);
      } else if (op < 0xf0) pc_=uint16_t((pc_&0xf000)|((op&15)<<8)|rom(pc_+1));
      else { stack_=uint16_t((pc_+2)&0xfff); pc_=uint16_t((pc_&0xf000)|((op&15)<<8)|rom(pc_+1)); }
      cycles=8;
   } else if (op >= 0x70) { acc_=op&15; next(); }
   else if (op >= 0x60) { wr_[2]=op&15; wr_[3]=imm(); pc_+=2; cycles=8; }
   else if (op >= 0x50) { wr_[0]=op&15; wr_[1]=imm(); pc_+=2; cycles=8; }
   else if (op >= 0x40) {
      switch(op) {
      case 0x40: add(imm(),0); break; case 0x41: sub(imm()); break;
      case 0x42: acc_&=imm(); break; case 0x43: acc_^=imm(); break; case 0x44: acc_|=imm(); break;
      case 0x45: sound_channel(imm()); break;
      case 0x46: wr_[4]=imm(); break; case 0x47: tc_=rom(pc_+1); break;
      case 0x48: sound_repeat_=false; next(); cycles=4; break;
      case 0x49: sound_repeat_=true; next(); cycles=4; break;
      case 0x4a: sound_off(); next(); cycles=4; break;
      case 0x4b: sound_channel(acc_); next(); cycles=4; break;
      case 0x4c: { ++pc_; uint8_t b=rom((pc_&0xff00)|(acc_<<4)|rb(0)); acc_=b&15; wr_[4]=b>>4; cycles=8; break; }
      case 0x4d: { ++pc_; uint8_t b=rom((pc_&0xf000)|0xf00|(acc_<<4)|rb(0)); acc_=b&15; wr_[4]=b>>4; cycles=8; break; }
      case 0x4e: { ++pc_; uint8_t b=rom((pc_&0xff00)|(acc_<<4)|wr_[4]); acc_=b&15; wb(0,b>>4); cycles=8; break; }
      case 0x4f: { ++pc_; uint8_t b=rom((pc_&0xf000)|0xf00|(acc_<<4)|wr_[4]); acc_=b&15; wb(0,b>>4); cycles=8; break; }
      default: break; // Sound control opcodes are handled by the audio stage.
      }
      if (op < 0x48) { pc_+=2; cycles=8; }
   } else {
      switch(op) {
      case 0x00: cf_=(acc_&1)!=0; acc_=uint8_t((unsigned(cf_)<<3)|(acc_>>1)); next(); break;
      case 0x01: cf_=(acc_>>3)!=0; acc_=uint8_t(unsigned(cf_)|((acc_<<1)&15)); next(); break;
      case 0x02: {bool n=(acc_&1)!=0; acc_=uint8_t((unsigned(cf_)<<3)|(acc_>>1)); cf_=n; next(); break;}
      case 0x03: {bool n=(acc_>>3)!=0; acc_=uint8_t(unsigned(cf_)|((acc_<<1)&15)); cf_=n; next(); break;}
      case 0x04: acc_=rb(0); next(); break; case 0x05: wb(0,acc_); next(); break;
      case 0x06: acc_=rb(2); next(); break; case 0x07: wb(2,acc_); next(); break;
      case 0x08: add(rb(0),cf_); next(); break; case 0x09: add(rb(0),0); next(); break;
      case 0x0a: {unsigned n=acc_+((~rb(0))&15)+cf_; cf_=n>15; acc_=n&15; next(); break;}
      case 0x0b: sub(rb(0)); next(); break;
      case 0x0c: wb(0,rb(0)+1); next(); break; case 0x0d: wb(0,rb(0)-1); next(); break;
      case 0x0e: wb(2,rb(2)+1); next(); break; case 0x0f: wb(2,rb(2)-1); next(); break;
      case 0x1a: acc_&=rb(0); next(); break; case 0x1b: acc_^=rb(0); next(); break; case 0x1c: acc_|=rb(0); next(); break;
      case 0x1d: wb(0,rb(0)&acc_); next(); break; case 0x1e: wb(0,rb(0)^acc_); next(); break; case 0x1f: wb(0,rb(0)|acc_); next(); break;
      case 0x2a: cf_=false; next(); break; case 0x2b: cf_=true; next(); break;
      case 0x2c: ei_=true; next(); break; case 0x2d: ei_=false; next(); break;
      case 0x2e: pc_=uint16_t((pc_&0xf000)|(stack_&0xfff)); stack_=0; break;
      case 0x2f: pc_=uint16_t((pc_&0xf000)|(stack_&0xfff)); cf_=stack_>>12; stack_=0; break;
      case 0x30: pa_=acc_; next(); break;
      case 0x31: acc_=(acc_+1)&15; next(); break; case 0x32: acc_=pm_; next(); break; case 0x33: acc_=ps_; next(); break; case 0x34: acc_=pp_; next(); break;
      case 0x36: if(acc_>9||cf_){acc_=(acc_+6)&15;cf_=true;} next(); break;
      case 0x37: pc_+=2; halt_=true; ef_=false; sound_off(); cycles=8; break;
      case 0x38: timer_on_=true; next(); break; case 0x39: timer_on_=false; next(); break;
      case 0x3a: acc_=tc_&15; next(); break; case 0x3b: acc_=tc_>>4; next(); break;
      case 0x3c: tc_=uint8_t((tc_&0xf0)|acc_); next(); break; case 0x3d: tc_=uint8_t((tc_&15)|(acc_<<4)); next(); break;
      case 0x3f: acc_=(acc_-1)&15; next(); break;
      default:
         if(op>=0x10&&op<=0x19){unsigned i=(op>>1)&7; if(op&1) wr_[i]=(wr_[i]-1)&15; else wr_[i]=(wr_[i]+1)&15; next();}
         else if(op>=0x20&&op<=0x29){unsigned i=(op>>1)&7; if(op&1) acc_=wr_[i]; else wr_[i]=acc_; next();}
         else next();
      }
   }
   sound_clock(cycles);
   timer_counter_ -= int(cycles);
   while (timer_counter_ <= 0) { timer_counter_ += int(timer_div()); if(timer_on_ && ++tc_ == 0) tf_=true; }
   ++instructions_;
   return cycles;
}
