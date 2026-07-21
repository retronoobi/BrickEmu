#include "libretro_175.h"
#include "ht943.hpp"
#include "em73000.hpp"
#include "spl0x.hpp"
#include "ks56cx2x.hpp"
#include "generated_lcd_masks.hpp"
#include "generated_apollo_lcd_masks.hpp"
#include "generated_ht943_extra_lcd_masks.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace {
constexpr unsigned MAX_WIDTH=624, MAX_HEIGHT=892, FPS=60, RATE=44100;
retro_environment_t env_cb=nullptr;
retro_video_refresh_t video_cb=nullptr;
retro_audio_sample_t audio_cb=nullptr;
retro_audio_sample_batch_t audio_batch_cb=nullptr;
retro_input_poll_t input_poll_cb=nullptr;
retro_input_state_t input_state_cb=nullptr;
retro_log_printf_t log_cb=nullptr;
enum class GameKind { E23, E88, E33, Apollo18, Apollo126, Keychain55, GA888, GA878, MiconKC32 };
GameKind game_kind=GameKind::E23;
HT943 ht943;
EM73000 em73000;
SPL0X spl03(SPL0X::Variant::SPL03),spl02(SPL0X::Variant::SPL02);
KS56CX2X ks56cx2x;
bool loaded=false;
bool auto_wake=true;
uint64_t cycle_fraction=0;
int64_t cycle_balance=0;
std::array<uint32_t, MAX_WIDTH*MAX_HEIGHT> frame{};
std::array<int16_t, (RATE/FPS)*2> audio_buffer{};
const lcd_masks::Layout *layout=&lcd_masks::e23;
double audio_phase=0.0;
uint32_t noise_lfsr=0x1aceu;
int noise_polarity=1;
std::array<double,4> apollo_phase{};
std::array<uint32_t,4> apollo_lfsr{{0x1357,0x2468,0x1ace,0x5eed}};
std::array<int,4> apollo_polarity{{1,1,1,1}};

bool is_ks56() { return game_kind==GameKind::GA878||game_kind==GameKind::MiconKC32; }

struct LegacySaveState {
   uint32_t magic,version,kind;
   uint64_t cycle_fraction;
   int64_t cycle_balance;
   double audio_phase;
   uint32_t noise_lfsr;
   int32_t noise_polarity;
   HT943::Snapshot ht943;
   EM73000::Snapshot em73000;
};
struct ApolloSaveState {
   uint32_t magic,version,kind;
   uint64_t cycle_fraction;
   int64_t cycle_balance;
   std::array<double,4> apollo_phase;
   std::array<uint32_t,4> apollo_lfsr;
   std::array<int32_t,4> apollo_polarity;
   SPL0X::Snapshot spl0x;
};
struct KS56SaveState {
   uint32_t magic,version,kind;
   uint64_t cycle_fraction;
   int64_t cycle_balance;
   KS56CX2X::Snapshot cpu;
};
constexpr uint32_t SAVE_MAGIC=0x42524b53u; // BRKS
constexpr uint32_t SAVE_VERSION=1;

uint32_t crc32(const uint8_t *p, size_t n) {
   uint32_t c=~0u;
   while(n--){c^=*p++; for(int k=0;k<8;k++) c=(c>>1)^(0xedb88320u&uint32_t(-(int)(c&1)));}
   return ~c;
}
void message(const char *s) { if(env_cb){retro_message m{s,180}; env_cb(RETRO_ENVIRONMENT_SET_MESSAGE,&m);} }
void update_variables() {
   if(!env_cb)return;
   retro_variable variable{"brickemu_auto_wake",nullptr};
   if(env_cb(RETRO_ENVIRONMENT_GET_VARIABLE,&variable)&&variable.value)auto_wake=std::strcmp(variable.value,"disabled")!=0;
}
void draw_segments(bool active_pass, uint32_t color) {
   for(size_t i=0;i<layout->segment_count;i++) {
      const auto &segment=layout->segments[i];
      uint8_t value;
      if(game_kind==GameKind::E33)value=em73000.vram()[segment.ram];
      else if(game_kind==GameKind::Apollo18)value=spl03.vram()[segment.ram];
      else if(game_kind==GameKind::Apollo126)value=spl02.vram()[segment.ram];
      else if(is_ks56())value=ks56cx2x.vram()[segment.ram];
      else value=ht943.vram()[segment.ram];
      bool active=(value&(1u<<segment.bit))!=0;
      if(active!=active_pass)continue;
      for(unsigned j=0;j<segment.count;j++) {
         const auto &span=layout->spans[segment.first+j];
         for(unsigned x=span.x0;x<span.x1;x++)frame[span.y*layout->width+x]=color;
      }
   }
}
void draw_fixed_lcd(uint32_t color) {
   for(size_t i=0;i<layout->fixed_span_count;i++) {
      const auto &span=layout->fixed_spans[i];
      for(unsigned x=span.x0;x<span.x1;x++)frame[span.y*layout->width+x]=color;
   }
}
void render_lcd() {
   // Match the original SVG LCD glass and its 2% ghost-segment opacity.
   std::fill_n(frame.begin(),layout->width*layout->height,0xffc3cacc);
   draw_segments(false,0xffc0c6c8);
   draw_segments(true,0xff172018);
   // Fixed glass markings must remain continuous above the segment ghosting.
   draw_fixed_lcd(0xff172018);
}
void update_input() {
   if(!input_state_cb) return;
   const unsigned ids[8]={RETRO_DEVICE_ID_JOYPAD_LEFT,RETRO_DEVICE_ID_JOYPAD_RIGHT,
      RETRO_DEVICE_ID_JOYPAD_DOWN,RETRO_DEVICE_ID_JOYPAD_A,RETRO_DEVICE_ID_JOYPAD_START,
      RETRO_DEVICE_ID_JOYPAD_SELECT,RETRO_DEVICE_ID_JOYPAD_X,RETRO_DEVICE_ID_JOYPAD_L};
   for(unsigned i=0;i<8;i++) {
      bool pressed=input_state_cb(0,RETRO_DEVICE_JOYPAD,0,ids[i])!=0;
      if(game_kind==GameKind::E33)em73000.set_button(i,pressed);
      else if(game_kind==GameKind::Apollo18)spl03.set_button(i,pressed);
      else if(game_kind==GameKind::Apollo126)spl02.set_button(i,pressed);
      else if(is_ks56())ks56cx2x.set_button(i,pressed);
      else ht943.set_button(i,pressed);
   }
}
void reset_timing() {
   cycle_fraction=0; cycle_balance=0; audio_phase=0.0;
   noise_lfsr=0x1aceu; noise_polarity=1;
   apollo_phase.fill(0);apollo_lfsr={0x1357,0x2468,0x1ace,0x5eed};apollo_polarity={1,1,1,1};
}
void run_audio_frame() {
   constexpr double pi=3.14159265358979323846;
   for(unsigned sample=0;sample<RATE/FPS;sample++) {
      bool apollo=game_kind==GameKind::Apollo18||game_kind==GameKind::Apollo126;
      unsigned system_clock=game_kind==GameKind::E33?2000000:apollo?690000:is_ks56()?1000000:ht943.clock_hz();
      cycle_fraction+=system_clock;
      uint64_t target=cycle_fraction/RATE;
      cycle_fraction%=RATE;
      cycle_balance-=int64_t(target);
      while(cycle_balance<0){
         unsigned elapsed;
         if(game_kind==GameKind::E33)elapsed=em73000.clock();
         else if(game_kind==GameKind::Apollo18)elapsed=spl03.clock();
         else if(game_kind==GameKind::Apollo126)elapsed=spl02.clock();
         else if(is_ks56())elapsed=ks56cx2x.clock();
         else elapsed=ht943.clock();
         // A malformed or not-yet-implemented opcode must never lock retro_run.
         if(!elapsed){if(log_cb)log_cb(RETRO_LOG_ERROR,"BrickEmu: CPU returned zero cycles; advancing one cycle\n");elapsed=1;}
         cycle_balance+=elapsed;
      }
      if(apollo){
         const auto &channels=game_kind==GameKind::Apollo18?spl03.audio_channels():spl02.audio_channels();double mixed=0;
         for(unsigned ch=0;ch<4;ch++){if(ch==3)mixed+=channels[ch].amplitude*655.0;else if(channels[ch].frequency>0){mixed+=std::sin(apollo_phase[ch])*channels[ch].amplitude*655.0*apollo_polarity[ch];double next=apollo_phase[ch]+2*pi*channels[ch].frequency/RATE;if(channels[ch].noise&&int(apollo_phase[ch]/pi)!=int(next/pi)){unsigned fb=((apollo_lfsr[ch]>>0)^(apollo_lfsr[ch]>>2)^(apollo_lfsr[ch]>>3)^(apollo_lfsr[ch]>>5))&1;apollo_lfsr[ch]=(apollo_lfsr[ch]>>1)|(fb<<15);apollo_polarity[ch]=(apollo_lfsr[ch]&1)?1:-1;}apollo_phase[ch]=std::fmod(next,2*pi);}}
         int16_t value=int16_t(std::max(-655,std::min(655,int(mixed))));audio_buffer[sample*2]=audio_buffer[sample*2+1]=value;continue;
      }
      if(is_ks56()){int16_t value=ks56cx2x.sound_level()?655:0;audio_buffer[sample*2]=audio_buffer[sample*2+1]=value;continue;}
      double frequency=game_kind==GameKind::E33?em73000.tone_frequency():ht943.tone_frequency();
      int16_t value=0;
      if(frequency>0.0) {
         double wave=std::sin(audio_phase);
         int mixed=int(wave*3275.0)*noise_polarity;
         value=int16_t(std::max(-655,std::min(655,mixed)));
         double next=audio_phase+2.0*pi*frequency/RATE;
         bool noise=game_kind==GameKind::E33?em73000.tone_noise():ht943.tone_noise();
         if(noise&&int(audio_phase/pi)!=int(next/pi)) {
            unsigned feedback=((noise_lfsr>>0)^(noise_lfsr>>2)^(noise_lfsr>>3)^(noise_lfsr>>5))&1;
            noise_lfsr=(noise_lfsr>>1)|(feedback<<15);
            noise_polarity=(noise_lfsr&1)?1:-1;
         }
         audio_phase=std::fmod(next,2.0*pi);
      }
      audio_buffer[sample*2]=audio_buffer[sample*2+1]=value;
   }
}
}

extern "C" {
RETRO_API unsigned retro_api_version(void){return RETRO_API_VERSION;}
RETRO_API void retro_set_environment(retro_environment_t cb){env_cb=cb;if(env_cb){static const retro_variable variables[]={
   {"brickemu_auto_wake","Prevent automatic power-off; enabled|disabled"},{nullptr,nullptr}};env_cb(RETRO_ENVIRONMENT_SET_VARIABLES,(void*)variables);}}
RETRO_API void retro_set_video_refresh(retro_video_refresh_t cb){video_cb=cb;}
RETRO_API void retro_set_audio_sample(retro_audio_sample_t cb){audio_cb=cb;}
RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb){audio_batch_cb=cb;}
RETRO_API void retro_set_input_poll(retro_input_poll_t cb){input_poll_cb=cb;}
RETRO_API void retro_set_input_state(retro_input_state_t cb){input_state_cb=cb;}
RETRO_API void retro_init(void){
   update_variables();
   if(env_cb){retro_log_callback l{}; if(env_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE,&l))log_cb=l.log;
      retro_pixel_format f=RETRO_PIXEL_FORMAT_XRGB8888; env_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT,&f);
      static const retro_input_descriptor d[]={
       {0,RETRO_DEVICE_JOYPAD,0,RETRO_DEVICE_ID_JOYPAD_LEFT,"Left"},{0,RETRO_DEVICE_JOYPAD,0,RETRO_DEVICE_ID_JOYPAD_RIGHT,"Right"},
       {0,RETRO_DEVICE_JOYPAD,0,RETRO_DEVICE_ID_JOYPAD_DOWN,"Down"},{0,RETRO_DEVICE_JOYPAD,0,RETRO_DEVICE_ID_JOYPAD_A,"Rotate"},
       {0,RETRO_DEVICE_JOYPAD,0,RETRO_DEVICE_ID_JOYPAD_START,"Start"},{0,RETRO_DEVICE_JOYPAD,0,RETRO_DEVICE_ID_JOYPAD_SELECT,"On/Off"},
       {0,RETRO_DEVICE_JOYPAD,0,RETRO_DEVICE_ID_JOYPAD_X,"Mute"},{0,RETRO_DEVICE_JOYPAD,0,RETRO_DEVICE_ID_JOYPAD_L,"Reset"},{0,0,0,0,nullptr}};
      env_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS,(void*)d);
   }
}
RETRO_API void retro_deinit(void){loaded=false;}
RETRO_API void retro_get_system_info(retro_system_info *i){std::memset(i,0,sizeof(*i));i->library_name="BrickEmu";i->library_version="1.0.1";i->valid_extensions="bin";i->need_fullpath=false;i->block_extract=false;}
RETRO_API void retro_get_system_av_info(retro_system_av_info *i){i->geometry={layout->width,layout->height,MAX_WIDTH,MAX_HEIGHT,float(layout->width)/layout->height};i->timing={FPS,RATE};}
RETRO_API void retro_set_controller_port_device(unsigned,unsigned){}
RETRO_API void retro_reset(void){if(loaded){if(game_kind==GameKind::E33)em73000.reset();else if(game_kind==GameKind::Apollo18)spl03.reset();else if(game_kind==GameKind::Apollo126)spl02.reset();else if(is_ks56())ks56cx2x.reset();else ht943.reset();}reset_timing();}
RETRO_API bool retro_load_game(const retro_game_info *g){
   if(!g||!g->data||(g->size!=2048&&g->size!=4096&&g->size!=8192&&g->size!=12288&&g->size!=20480)){message("BrickEmu: unsupported ROM size");return false;}
   uint32_t crc=crc32((const uint8_t*)g->data,g->size);
   if(crc==0x8045fac4u){game_kind=GameKind::E23;layout=&lcd_masks::e23;loaded=ht943.load(g->data,g->size);}
   else if(crc==0xf9ce0a69u){game_kind=GameKind::E88;layout=&lcd_masks::e88;loaded=ht943.load(g->data,g->size);}
   else if(crc==0xea032a7au){game_kind=GameKind::E33;layout=&lcd_masks::e33;loaded=em73000.load(g->data,g->size);}
   else if(crc==0x53340597u){game_kind=GameKind::Apollo18;layout=&lcd_masks::apollo18;loaded=spl03.load(g->data,g->size);}
   else if(crc==0x85879d7cu){game_kind=GameKind::Apollo126;layout=&lcd_masks::apollo126;loaded=spl02.load(g->data,g->size);}
   else if(crc==0xc8623cf2u){game_kind=GameKind::Keychain55;layout=&lcd_masks::keychain55;loaded=ht943.load(g->data,g->size,HT943::Profile::Keychain55);}
   else if(crc==0xcb3f8ff4u){game_kind=GameKind::GA888;layout=&lcd_masks::ga888;loaded=ht943.load(g->data,g->size,HT943::Profile::GA888);}
   else if(crc==0x9fd44d5cu){game_kind=GameKind::GA878;layout=&lcd_masks::ga878;loaded=ks56cx2x.load(g->data,g->size);}
   else if(crc==0x8ccbfe1au){game_kind=GameKind::MiconKC32;layout=&lcd_masks::miconkc32;loaded=ks56cx2x.load(g->data,g->size,KS56CX2X::Profile::MiconKC32);}
   else {message("BrickEmu: unknown ROM");return false;}
   reset_timing();
   if(log_cb)log_cb(RETRO_LOG_INFO,"BrickEmu: loaded ROM CRC32 %08x\n",crc);
   return loaded;
}
RETRO_API bool retro_load_game_special(unsigned,const retro_game_info*,size_t){return false;}
RETRO_API void retro_unload_game(void){loaded=false;}
RETRO_API unsigned retro_get_region(void){return 0;}
RETRO_API void *retro_get_memory_data(unsigned){return nullptr;}
RETRO_API size_t retro_get_memory_size(unsigned){return 0;}
RETRO_API size_t retro_serialize_size(void){
   if(!loaded)return 0;
   if(is_ks56())return sizeof(KS56SaveState);
   return (game_kind==GameKind::Apollo18||game_kind==GameKind::Apollo126)?sizeof(ApolloSaveState):sizeof(LegacySaveState);
}
RETRO_API bool retro_serialize(void *data,size_t size){
   if(!loaded||!data)return false;
   if(is_ks56()){
      if(size<sizeof(KS56SaveState))return false;KS56SaveState state{};state.magic=SAVE_MAGIC;state.version=SAVE_VERSION;state.kind=uint32_t(game_kind);
      state.cycle_fraction=cycle_fraction;state.cycle_balance=cycle_balance;ks56cx2x.save(state.cpu);std::memcpy(data,&state,sizeof(state));return true;
   }
   if(game_kind==GameKind::Apollo18||game_kind==GameKind::Apollo126){
      if(size<sizeof(ApolloSaveState))return false;
      ApolloSaveState state{};state.magic=SAVE_MAGIC;state.version=SAVE_VERSION;state.kind=uint32_t(game_kind);
      state.cycle_fraction=cycle_fraction;state.cycle_balance=cycle_balance;
      state.apollo_phase=apollo_phase;state.apollo_lfsr=apollo_lfsr;
      std::copy(apollo_polarity.begin(),apollo_polarity.end(),state.apollo_polarity.begin());
      if(game_kind==GameKind::Apollo18)spl03.save(state.spl0x);else spl02.save(state.spl0x);
      std::memcpy(data,&state,sizeof(state));return true;
   }
   if(size<sizeof(LegacySaveState))return false;
   LegacySaveState state{};state.magic=SAVE_MAGIC;state.version=SAVE_VERSION;state.kind=uint32_t(game_kind);
   state.cycle_fraction=cycle_fraction;state.cycle_balance=cycle_balance;state.audio_phase=audio_phase;
   state.noise_lfsr=noise_lfsr;state.noise_polarity=noise_polarity;
   if(game_kind==GameKind::E33)em73000.save(state.em73000);else ht943.save(state.ht943);
   std::memcpy(data,&state,sizeof(state));return true;
}
RETRO_API bool retro_unserialize(const void *data,size_t size){
   if(!loaded||!data)return false;
   if(is_ks56()){
      if(size<sizeof(KS56SaveState))return false;KS56SaveState state{};std::memcpy(&state,data,sizeof(state));
      if(state.magic!=SAVE_MAGIC||state.version!=SAVE_VERSION||state.kind!=uint32_t(game_kind))return false;
      cycle_fraction=state.cycle_fraction;cycle_balance=state.cycle_balance;ks56cx2x.restore(state.cpu);return true;
   }
   if(game_kind==GameKind::Apollo18||game_kind==GameKind::Apollo126){
      if(size<sizeof(ApolloSaveState))return false;
      ApolloSaveState state{};std::memcpy(&state,data,sizeof(state));
      if(state.magic!=SAVE_MAGIC||state.version!=SAVE_VERSION||state.kind!=uint32_t(game_kind))return false;
      cycle_fraction=state.cycle_fraction;cycle_balance=state.cycle_balance;
      apollo_phase=state.apollo_phase;apollo_lfsr=state.apollo_lfsr;
      std::copy(state.apollo_polarity.begin(),state.apollo_polarity.end(),apollo_polarity.begin());
      if(game_kind==GameKind::Apollo18)spl03.restore(state.spl0x);else spl02.restore(state.spl0x);
      return true;
   }
   if(size<sizeof(LegacySaveState))return false;
   LegacySaveState state{};std::memcpy(&state,data,sizeof(state));
   if(state.magic!=SAVE_MAGIC||state.version!=SAVE_VERSION||state.kind!=uint32_t(game_kind))return false;
   cycle_fraction=state.cycle_fraction;cycle_balance=state.cycle_balance;audio_phase=state.audio_phase;
   noise_lfsr=state.noise_lfsr;noise_polarity=state.noise_polarity;
   if(game_kind==GameKind::E33)em73000.restore(state.em73000);else ht943.restore(state.ht943);
   return true;
}
RETRO_API void retro_cheat_reset(void){}
RETRO_API void retro_cheat_set(unsigned,bool,const char*){}
RETRO_API void retro_run(void){
   bool variables_updated=false;if(env_cb&&env_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE,&variables_updated)&&variables_updated)update_variables();
   if(input_poll_cb)input_poll_cb(); update_input();
   bool woke=false;if(loaded&&auto_wake){if(game_kind==GameKind::E33)woke=em73000.wake_from_deep_sleep();else if(game_kind==GameKind::Apollo18)woke=spl03.wake_from_deep_sleep();else if(game_kind==GameKind::Apollo126)woke=spl02.wake_from_deep_sleep();else if(is_ks56())woke=ks56cx2x.wake_from_deep_sleep();else woke=ht943.wake_from_deep_sleep();}
   if(woke&&log_cb)log_cb(RETRO_LOG_INFO,"BrickEmu: automatic wake from idle sleep\n");
   if(loaded)run_audio_frame(); else audio_buffer.fill(0);
   render_lcd(); if(video_cb)video_cb(frame.data(),layout->width,layout->height,layout->width*sizeof(uint32_t));
   if(audio_batch_cb)audio_batch_cb(audio_buffer.data(),RATE/FPS); else if(audio_cb)for(unsigned i=0;i<RATE/FPS;i++)audio_cb(audio_buffer[i*2],audio_buffer[i*2+1]);
}
}
