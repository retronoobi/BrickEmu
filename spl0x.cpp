#include "spl0x.hpp"
#include <algorithm>

namespace{constexpr unsigned RESET=0x1ffc,IRQ=0x1ffe,NMI=0x1ffa;}
uint8_t SPL0X::rom(unsigned n)const{return rom_.empty()?0:rom_[n%rom_.size()];}
uint16_t SPL0X::rom_word_lsb(unsigned n)const{return uint16_t(rom(n)|(rom(n+1)<<8));}
unsigned SPL0X::mapped_pc()const{return pc_>0xfff?(pc_%0x2000)+(rom_bank_<<12):pc_;}
bool SPL0X::load(const void*d,std::size_t n){if(!d||(variant_==Variant::SPL03?n!=12288:n!=20480))return false;rom_.assign((const uint8_t*)d,(const uint8_t*)d+n);reset();return true;}
void SPL0X::go_vector(unsigned v){pc_=rom_word_lsb(v+(rom_bank_<<12));}
void SPL0X::reset(){t2_counter_=t256_counter_=0;pc_=sp_=a_=x_=0;set_ps(4);cpu_on_=rosc_on_=clock32_on_=true;ram_.fill(0);lcd_.fill(0);rom_bank_=0;pdir_=platch_=input_low_=input_high_=0;io_ctrl_=int_cfg_=ireq_=sys_ctrl_=0;prescaler_=1;speech_ctrl_=speech_data_=tonea1_=tonea2_=toneb1_=toneb2_=noise1_=noise2_=0;cycle_counter_=instructions_=0;audio_={};go_vector(RESET);}
void SPL0X::save(Snapshot&s)const{
   s.ram=ram_;s.lcd=lcd_;s.t2_counter=t2_counter_;s.t256_counter=t256_counter_;
   s.cycle_counter=cycle_counter_;s.instructions=instructions_;s.pc=pc_;s.sp=sp_;s.a=a_;s.x=x_;
   s.rom_bank=rom_bank_;s.pdir=pdir_;s.platch=platch_;s.input_low=input_low_;s.input_high=input_high_;
   s.io_ctrl=io_ctrl_;s.int_cfg=int_cfg_;s.ireq=ireq_;s.sys_ctrl=sys_ctrl_;s.prescaler=prescaler_;
   s.speech_ctrl=speech_ctrl_;s.speech_data=speech_data_;s.tonea1=tonea1_;s.tonea2=tonea2_;
   s.toneb1=toneb1_;s.toneb2=toneb2_;s.noise1=noise1_;s.noise2=noise2_;
   s.flags=uint16_t(unsigned(nf_)|(unsigned(vf_)<<1)|(unsigned(df_)<<2)|(unsigned(bf_)<<3)|
      (unsigned(if_)<<4)|(unsigned(zf_)<<5)|(unsigned(cf_)<<6)|(unsigned(cpu_on_)<<7)|
      (unsigned(rosc_on_)<<8)|(unsigned(clock32_on_)<<9));
}
void SPL0X::restore(const Snapshot&s){
   ram_=s.ram;lcd_=s.lcd;t2_counter_=s.t2_counter;t256_counter_=s.t256_counter;
   cycle_counter_=s.cycle_counter;instructions_=s.instructions;pc_=s.pc;sp_=s.sp;a_=s.a;x_=s.x;
   rom_bank_=s.rom_bank;pdir_=s.pdir;platch_=s.platch;input_low_=s.input_low;input_high_=s.input_high;
   io_ctrl_=s.io_ctrl;int_cfg_=s.int_cfg;ireq_=s.ireq;sys_ctrl_=s.sys_ctrl;prescaler_=s.prescaler;
   speech_ctrl_=s.speech_ctrl;speech_data_=s.speech_data;tonea1_=s.tonea1;tonea2_=s.tonea2;
   toneb1_=s.toneb1;toneb2_=s.toneb2;noise1_=s.noise1;noise2_=s.noise2;
   nf_=s.flags&1;vf_=s.flags&2;df_=s.flags&4;bf_=s.flags&8;if_=s.flags&16;
   zf_=s.flags&32;cf_=s.flags&64;cpu_on_=s.flags&128;rosc_on_=s.flags&256;clock32_on_=s.flags&512;
   update_audio();
}
uint8_t SPL0X::port_read()const{return uint8_t((~pdir_&platch_)|(pdir_&(~input_low_&(input_high_))));}
uint8_t SPL0X::get_io(unsigned p){switch(p){case 0xc0:return io_ctrl_;case 0xc1:return port_read();case 0xd0:return sys_ctrl_;case 0xd2:{uint8_t b=ireq_|(int_cfg_&0x80);ireq_=0;return b;}case 0xd4:return speech_ctrl_;case 0xd5:return(cycle_counter_>>3)&1;case 0xd7:return rom_bank_;default:return 0;}}
void SPL0X::update_audio(){constexpr double factor=690000.0/16.0/32768.0;auto tone=[&](unsigned i,uint8_t c1,uint8_t c2){double vol=(c2>>6)/3.0;unsigned f=(((c2&3)<<8)|c1)<<1;audio_[i]={vol&&f?factor*f:0,vol,false};};tone(0,tonea1_,tonea2_);tone(1,toneb1_,toneb2_);double nv=(noise2_>>4)/15.0;unsigned nf=(16-(noise1_&15))<<7;audio_[2]={nv&&nf?factor*nf:0,nv,true};double amp=((speech_data_&0x3f)/63.0)*(1-((speech_data_>>6)&2));if((speech_data_==0)||!(speech_ctrl_&1))amp=0;audio_[3]={0,amp,false};}
void SPL0X::set_io(unsigned p,uint8_t v){switch(p){case 0xc0:if(v&1)pdir_|=15;if(v&2)pdir_|=0xf0;rosc_on_=((io_ctrl_|~v)&0x10)!=0;prescaler_=(v&0x20)?8:1;io_ctrl_=v;break;case 0xc1:platch_=v;break;case 0xc4:tonea1_=v;update_audio();break;case 0xc6:tonea2_=v;update_audio();break;case 0xc8:if(variant_==Variant::SPL02){toneb1_=v;update_audio();}break;case 0xca:if(variant_==Variant::SPL02){toneb2_=v;update_audio();}break;case 0xcc:noise1_=v;update_audio();break;case 0xce:noise2_=v;update_audio();break;case 0xd0:rosc_on_=((sys_ctrl_|~v)&0x80)!=0;cpu_on_=((sys_ctrl_|~v)&0x40)!=0;sys_ctrl_=v;break;case 0xd2:int_cfg_=v;break;case 0xd4:speech_ctrl_=v;update_audio();break;case 0xd5:speech_data_=variant_==Variant::SPL03?(v&0xbe):v;update_audio();break;case 0xd7:rom_bank_=variant_==Variant::SPL03?(v&1):v;break;default:break;}}
void SPL0X::write(unsigned p,uint8_t v){unsigned ramsz=variant_==Variant::SPL03?0x50:0x90;if(p<0x30)lcd_[p]=v;else if(p>=0x30&&p<0x30+ramsz)ram_[p-0x30]=v;else set_io(p,v);}
uint8_t SPL0X::read(unsigned p){unsigned ramsz=variant_==Variant::SPL03?0x50:0x90;if(p<0x30)return lcd_[p];if(p>=0x30&&p<0x30+ramsz)return ram_[p-0x30];if(p>=0xc0&&p<0x100)return get_io(p);if(p>=0x1000)p=(p%0x2000)+(rom_bank_<<12);return rom(p);}
uint8_t SPL0X::ps()const{return uint8_t((unsigned(nf_)<<7)|(unsigned(vf_)<<6)|(unsigned(bf_)<<4)|(unsigned(df_)<<3)|(unsigned(if_)<<2)|(unsigned(zf_)<<1)|unsigned(cf_));}
void SPL0X::set_ps(uint8_t p){nf_=p>>7;vf_=p&0x40;bf_=p&0x10;df_=p&8;if_=p&4;zf_=p&2;cf_=p&1;}
void SPL0X::irq(){write(sp_,pc_>>8);sp_--;write(sp_,pc_);sp_--;write(sp_,ps());sp_--;if_=true;go_vector(IRQ);}
void SPL0X::nmi(){if(int_cfg_&0x80){if(rosc_on_&&cpu_on_){write(sp_,pc_>>8);sp_--;write(sp_,pc_);sp_--;write(sp_,ps());sp_--;go_vector(NMI);}else{cpu_on_=rosc_on_=true;go_vector(RESET);}}}
void SPL0X::timers(unsigned c){t2_counter_-=c;while(t2_counter_<=0){t2_counter_+=16*(32768/2);if(int_cfg_&1){ireq_|=1;nmi();}}t256_counter_-=c;while(t256_counter_<=0){t256_counter_+=16*(32768/256);if(int_cfg_&2){ireq_|=2;nmi();}}}
void SPL0X::set_button(unsigned b,bool down){static const uint8_t masks[8]={8,16,4,2,64,1,32,0};if(b==7){if(down)reset();return;}if(b>=7)return;uint8_t prev=port_read(),m=masks[b];input_low_&=~m;input_high_&=~m;if(down)input_high_|=m;if(!(prev&1)&&(port_read()&1)&&(int_cfg_&4)){ireq_|=4;nmi();}}
bool SPL0X::wake_from_deep_sleep(){if(cpu_on_||rosc_on_)return false;set_button(5,true);return true;}
void SPL0X::adc(uint8_t o){int old=a_,n=old+o+cf_;if(df_&&((old&15)+(o&15)+cf_>9))n+=6;vf_=((~(old^o)&(old^n))>>7)&1;nf_=(n>>7)&1;if(df_&&n>0x99)n+=0x60;zf_=!(n&255);cf_=n>255;a_=n;}
void SPL0X::sbc(uint8_t o){int old=a_,n=old-o-!cf_;if(df_){if((old&15)-(o&15)-!cf_<0)n-=6;if(n<0)n-=0x60;}vf_=(((old^o)&(old^n))>>7)&1;nf_=(n>>7)&1;zf_=!(n&255);cf_=n>=0;a_=n;}

unsigned SPL0X::clock(){unsigned cyc=1;if(rosc_on_){if(cpu_on_){unsigned addr=mapped_pc();uint8_t op=rom(addr);unsigned count=1;switch(op){case 0x05:case 0x09:case 0x10:case 0x24:case 0x25:case 0x26:case 0x29:case 0x30:case 0x45:case 0x49:case 0x50:case 0x55:case 0x58:case 0x65:case 0x69:case 0x70:case 0x81:case 0x85:case 0x86:case 0x90:case 0x95:case 0xa1:case 0xa2:case 0xa5:case 0xa6:case 0xa9:case 0xb0:case 0xb5:case 0xc5:case 0xc6:case 0xc9:case 0xd0:case 0xd5:case 0xd6:case 0xe0:case 0xe4:case 0xe5:case 0xe6:case 0xe9:case 0xf0:count=2;break;case 0x20:case 0x2c:case 0x4c:case 0x6c:case 0x8e:case 0xad:case 0xae:case 0xbd:count=3;break;default:break;}uint32_t code=0;for(unsigned i=0;i<count;i++)code=(code<<8)|rom(addr+i);pc_+=count;uint8_t q=code&255;uint16_t ab=uint16_t(((code&255)<<8)|((code>>8)&255));auto flags=[&](uint8_t v){nf_=v>>7;zf_=!v;};auto branch=[&](bool take){if(take){uint16_t old=pc_;pc_=uint16_t(pc_+(int8_t)q);return unsigned(3+((pc_^old)>255));}return 2u;};switch(op){
case 0x00:bf_=1;pc_++;irq();cyc=7;break;case 0x05:a_|=read(q);flags(a_);cyc=3;break;case 0x08:write(sp_--,ps());cyc=3;break;case 0x09:a_|=q;flags(a_);cyc=2;break;case 0x10:cyc=branch(!nf_);break;case 0x18:cf_=0;cyc=2;break;case 0x20:{uint16_t r=pc_-1;write(sp_--,r>>8);write(sp_--,r);pc_=ab;cyc=6;break;}case 0x24:{uint8_t m=read(q);nf_=m>>7;vf_=m&64;zf_=!(a_&m);cyc=3;break;}case 0x25:a_&=read(q);flags(a_);cyc=3;break;case 0x26:{int n=(read(q)<<1)|unsigned(cf_);write(q,n);flags(n);cf_=n>255;cyc=5;break;}case 0x28:set_ps(read(++sp_));cyc=4;break;case 0x29:a_&=q;flags(a_);cyc=2;break;case 0x2a:{int n=(a_<<1)|unsigned(cf_);a_=n;flags(a_);cf_=n>255;cyc=2;break;}case 0x2c:{uint8_t m=read(ab);nf_=m>>7;vf_=m&64;zf_=!(a_&m);cyc=4;break;}case 0x30:cyc=branch(nf_);break;case 0x38:cf_=1;cyc=2;break;
case 0x40:set_ps(read(++sp_));pc_=read(++sp_);pc_|=read(++sp_)<<8;cyc=6;break;case 0x45:a_^=read(q);flags(a_);cyc=3;break;case 0x48:write(sp_--,a_);cyc=3;break;case 0x49:a_^=q;flags(a_);cyc=2;break;case 0x4c:pc_=ab;cyc=3;break;case 0x50:cyc=branch(!vf_);break;case 0x55:a_^=read((q+x_)&255);flags(a_);cyc=4;break;case 0x58:if_=0;cyc=2;break;
case 0x60:pc_=read(++sp_);pc_|=read(++sp_)<<8;pc_++;cyc=6;break;case 0x65:adc(read(q));cyc=3;break;case 0x66:{uint8_t v=read(q),c=v&1;write(q,(v>>1)|(unsigned(cf_)<<7));nf_=cf_;zf_=!((v&0xfe)|unsigned(cf_));cf_=c;cyc=5;break;}case 0x68:a_=read(++sp_);flags(a_);cyc=4;break;case 0x69:adc(q);cyc=2;break;case 0x6a:{uint8_t v=a_,c=v&1;a_=(v>>1)|(unsigned(cf_)<<7);nf_=cf_;zf_=!((v&0xfe)|unsigned(cf_));cf_=c;cyc=2;break;}case 0x6c:pc_=read(ab)|(read((ab+1)&0xffff)<<8);cyc=6;break;case 0x70:cyc=branch(vf_);break;case 0x78:if_=1;cyc=2;break;
case 0x81:{uint8_t zp=(q+x_)&255;write(read(zp)|(read((zp+1)&255)<<8),a_);cyc=6;break;}case 0x85:write(q,a_);cyc=3;break;case 0x86:write(q,x_);cyc=3;break;case 0x8a:a_=x_;flags(a_);cyc=2;break;case 0x8e:write(ab,x_);cyc=4;break;case 0x90:cyc=branch(!cf_);break;case 0x95:write((q+x_)&255,a_);cyc=4;break;case 0x9a:sp_=x_;cyc=2;break;
case 0xa1:{uint8_t zp=(q+x_)&255;a_=read(read(zp)|(read((zp+1)&255)<<8));flags(a_);cyc=6;break;}case 0xa2:x_=q;flags(x_);cyc=2;break;case 0xa5:a_=read(q);flags(a_);cyc=3;break;case 0xa6:x_=read(q);flags(x_);cyc=3;break;case 0xa9:a_=q;flags(a_);cyc=2;break;case 0xaa:x_=a_;flags(x_);cyc=2;break;case 0xad:a_=read(ab);flags(a_);cyc=4;break;case 0xae:x_=read(ab);flags(x_);cyc=4;break;case 0xb0:cyc=branch(cf_);break;case 0xb5:a_=read((q+x_)&255);flags(a_);cyc=4;break;case 0xb8:vf_=0;cyc=2;break;case 0xba:x_=sp_;flags(x_);cyc=2;break;case 0xbd:{uint16_t ad=ab+x_;a_=read(ad);flags(a_);cyc=4+((pc_^ad)>255);break;}
case 0xc5:{int t=a_-read(q);nf_=(t>>7)&1;zf_=!t;cf_=t>=0;cyc=3;break;}case 0xc6:{uint8_t v=read(q)-1;write(q,v);flags(v);cyc=5;break;}case 0xc9:{int t=a_-q;nf_=(t>>7)&1;zf_=!t;cf_=t>=0;cyc=2;break;}case 0xca:x_--;flags(x_);cyc=2;break;case 0xd0:cyc=branch(!zf_);break;case 0xd5:{int t=a_-read((q+x_)&255);nf_=(t>>7)&1;zf_=!t;cf_=t>=0;cyc=4;break;}case 0xd6:{uint8_t ad=q+x_,v=read(ad)-1;write(ad,v);flags(v);cyc=6;break;}
case 0xe0:{int t=x_-q;nf_=(t>>7)&1;zf_=!t;cf_=t>=0;cyc=2;break;}case 0xe4:{int t=x_-read(q);nf_=(t>>7)&1;zf_=!t;cf_=t>=0;cyc=3;break;}case 0xe5:sbc(read(q));cyc=3;break;case 0xe6:{uint8_t v=read(q)+1;write(q,v);flags(v);cyc=5;break;}case 0xe8:x_++;flags(x_);cyc=2;break;case 0xe9:sbc(q);cyc=2;break;case 0xea:cyc=2;break;case 0xf0:cyc=branch(zf_);break;case 0xf8:df_=1;cyc=2;break;default:cyc=2;break;}instructions_++;}timers(cyc);}else if(sys_ctrl_&0x20){cyc=16;timers(cyc);}cycle_counter_+=cyc;return cyc;}
