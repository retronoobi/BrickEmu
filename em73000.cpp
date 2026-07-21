#include "em73000.hpp"
#include <algorithm>

namespace {
constexpr unsigned STACK=0xc0;
constexpr unsigned timer_div[4]={1u<<10,1u<<14,1u<<18,1u<<22};
constexpr unsigned time_base_div[16]={0,0,0,0,1u<<10,1u<<11,1u<<12,1u<<13,0,0,0,0,1u<<9,1u<<8,1u<<15,1u<<17};
constexpr unsigned warmup[4]={1u<<18,1u<<14,1u<<16,8};
constexpr uint8_t mask_to_il[16]={0x20,0x21,0x26,0x27,0x28,0x29,0x2e,0x2f,0x30,0x31,0x36,0x37,0x38,0x39,0x3e,0x3f};
constexpr uint16_t interrupt_entries[6]={0x00c,0x00a,0x008,0x006,0x004,0x002};
}

bool EM73000::load(const void *data,std::size_t size){
   if(!data||size!=8192)return false;
   rom_.assign((const uint8_t*)data,(const uint8_t*)data+size);reset();return true;
}
uint8_t EM73000::rom(unsigned a)const{return rom_.empty()?0:rom_[a%rom_.size()];}
uint16_t EM73000::word(unsigned a)const{return uint16_t((rom(a)<<8)|rom(a+1));}

void EM73000::reset(){
   ram_.fill(0);pc_=dp_=timer_a_=timer_b_=0;acc_=sp_=hl_=il_=imask_=0;
   ei_=cf_=zf_=false;sf_=true;p0_=p7_=p8_=15;p4_=p5_=p6_=p9_=p16_=0;
   p23_=p24_=15;p25_=p27_=p28_=p29_=p30_=0;timer_a_counter_=timer_b_counter_=time_base_counter_=0;
   ram_bank_=0;sound_basic_div_=sound_freq_div_=sound_mode_=0;tone_frequency_=0;tone_noise_=false;instructions_=0;
}

void EM73000::save(Snapshot& s) const {
   s.ram=ram_;s.pc=pc_;s.dp=dp_;s.timer_a=timer_a_;s.timer_b=timer_b_;s.instructions=instructions_;
   s.timer_a_counter=timer_a_counter_;s.timer_b_counter=timer_b_counter_;s.time_base_counter=time_base_counter_;
   s.tone_frequency=tone_frequency_;s.ram_bank=ram_bank_;s.sound_basic_div=sound_basic_div_;
   s.sound_freq_div=sound_freq_div_;s.sound_mode=sound_mode_;s.acc=acc_;s.sp=sp_;s.hl=hl_;s.il=il_;s.imask=imask_;
   const uint8_t p[15]={p0_,p4_,p5_,p6_,p7_,p8_,p9_,p16_,p23_,p24_,p25_,p27_,p28_,p29_,p30_};
   std::copy(p,p+15,s.ports);s.flags=uint16_t(unsigned(ei_)|(unsigned(cf_)<<1)|(unsigned(zf_)<<2)|
                                              (unsigned(sf_)<<3)|(unsigned(tone_noise_)<<4));
}
void EM73000::restore(const Snapshot& s) {
   ram_=s.ram;pc_=s.pc;dp_=s.dp;timer_a_=s.timer_a;timer_b_=s.timer_b;instructions_=s.instructions;
   timer_a_counter_=s.timer_a_counter;timer_b_counter_=s.timer_b_counter;time_base_counter_=s.time_base_counter;
   tone_frequency_=s.tone_frequency;ram_bank_=s.ram_bank;sound_basic_div_=s.sound_basic_div;
   sound_freq_div_=s.sound_freq_div;sound_mode_=s.sound_mode;acc_=s.acc;sp_=s.sp;hl_=s.hl;il_=s.il;imask_=s.imask;
   p0_=s.ports[0];p4_=s.ports[1];p5_=s.ports[2];p6_=s.ports[3];p7_=s.ports[4];p8_=s.ports[5];
   p9_=s.ports[6];p16_=s.ports[7];p23_=s.ports[8];p24_=s.ports[9];p25_=s.ports[10];
   p27_=s.ports[11];p28_=s.ports[12];p29_=s.ports[13];p30_=s.ports[14];
   ei_=s.flags&1;cf_=s.flags&2;zf_=s.flags&4;sf_=s.flags&8;tone_noise_=s.flags&16;
}

uint8_t EM73000::get_io(unsigned p)const{
   switch(p){case 0:return p0_;case 4:return p4_;case 7:return p7_;case 8:return p8_;default:return 0;}
}
void EM73000::update_sound(){
   if(sound_mode_&&sound_freq_div_>1&&sound_basic_div_){tone_frequency_=2000000.0/sound_basic_div_/sound_freq_div_/2.0;tone_noise_=sound_mode_!=1;}
   else tone_frequency_=0.0;
}
void EM73000::set_io(unsigned p,uint8_t v){
   v&=15;
   switch(p){
   case 4:p4_=v;break;case 5:p5_=v;break;case 6:p6_=v;break;
   case 9:p9_=v;ram_bank_=(v>>3)*256;break;case 16:p16_=v;break;
   case 23:p23_=v;sound_freq_div_=((p24_<<4)|v)+1;update_sound();break;
   case 24:p24_=v;break;case 25:p25_=v;break;case 27:p27_=v;break;
   case 28:p28_=v;timer_a_counter_=0;break;case 29:p29_=v;timer_b_counter_=0;break;
   case 30:p30_=v;sound_basic_div_=4u<<((v&12)>>2);sound_mode_=v&3;update_sound();break;
   default:break;
   }
}
void EM73000::set_pin(unsigned p,unsigned pin,bool pressed){
   uint8_t mask=uint8_t(1u<<pin);
   uint8_t *port=p==0?&p0_:p==7?&p7_:&p8_;
   uint8_t previous=*port;
   if(pressed)*port&=uint8_t(~mask);else *port|=mask;
   if(p==0&&pin==0&&(previous&mask)&&!(*port&mask))p16_&=uint8_t(~4);
   if(p==8&&pressed&&(previous&mask)&&!(*port&mask)){
      if(pin==0)il_|=1;if(pin==2)il_|=32;
   }
}
void EM73000::set_button(unsigned b,bool pressed){
   static const uint8_t ports[7]={7,7,7,7,8,0,8};
   static const uint8_t pins[7]={2,3,1,0,1,0,3};
   if(b<7)set_pin(ports[b],pins[b],pressed);
}
bool EM73000::wake_from_deep_sleep(){if(!(p16_&4))return false;set_pin(0,0,true);return true;}

void EM73000::process_timers(unsigned c){
   if((p28_&12)==8){timer_a_counter_-=c;if(timer_a_counter_<=0){timer_a_counter_+=timer_div[p28_&3];timer_a_=(timer_a_+1)&0xfff;if(!timer_a_)il_|=8;}}
   if((p29_&12)==8){timer_b_counter_-=c;if(timer_b_counter_<=0){timer_b_counter_+=timer_div[p29_&3];timer_b_=(timer_b_+1)&0xfff;if(!timer_b_)il_|=4;}}
   if(time_base_div[p25_]){time_base_counter_-=c;if(time_base_counter_<=0){timer_b_counter_+=time_base_div[p25_];il_|=2;}}
}
void EM73000::interrupt(){
   for(unsigned id=0;id<6;id++)if(il_&(0x20>>id)&mask_to_il[imask_]){
      unsigned s=STACK+(sp_<<2);ram_[s]=uint8_t((cf_<<3)|(zf_<<2)|(sf_<<1)|((pc_>>12)&1));
      ram_[s+1]=(pc_>>8)&15;ram_[s+2]=(pc_>>4)&15;ram_[s+3]=pc_&15;sp_=(sp_-1)&15;
      pc_=interrupt_entries[5-id];sf_=true;ei_=false;il_&=uint8_t(~(0x20>>id));return;
   }
}

unsigned EM73000::clock(){
   if(ei_)interrupt();
   if(p16_&4)return warmup[p16_&3];
   uint8_t op=rom(pc_),arg=rom((pc_+1)&0x1fff);unsigned cycles=8;
   auto next=[&](unsigned n=1){pc_=(pc_+n)&0x1fff;};
   auto mem=[&]()->uint8_t&{return ram_[ram_bank_+hl_];};
   auto stack_push=[&](unsigned ret){unsigned s=STACK+(sp_<<2);ram_[s]=ret>>12;ram_[s+1]=(ret>>8)&15;ram_[s+2]=(ret>>4)&15;ram_[s+3]=ret&15;sp_=(sp_-1)&15;};
   auto stack_pop=[&](){sp_=(sp_+1)&15;unsigned s=STACK+(sp_<<2);return unsigned(((ram_[s]&1)<<12)|(ram_[s+1]<<8)|(ram_[s+2]<<4)|ram_[s+3]);};

   if(op<0x40){next();if(sf_)pc_=(pc_&0x1fc0)|(op&0x3f);sf_=true;}
   else if(op<0x48){stack_push(pc_+2);pc_=((op&7)<<8)|arg;cycles=16;}
   else if(op>=0x80&&op<0x90){hl_=(hl_&0xf0)|(op&15);sf_=true;next();}
   else if(op>=0x90&&op<0xa0){hl_=(hl_&15)|((op&15)<<4);sf_=true;next();}
   else if(op>=0xa0&&op<0xb0){mem()=op&15;hl_=(hl_&0xf0)|((hl_+1)&15);zf_=(hl_&15)==0;sf_=(hl_&15)!=0;next();}
   else if(op>=0xb0&&op<0xc0){unsigned k=op&15;cf_=k>=acc_;zf_=k==acc_;sf_=!zf_;next();}
   else if(op>=0xc0&&op<0xd0){next(2);if(sf_)pc_=(pc_&0x1000)|(((op<<8)|arg)&0xfff);sf_=true;cycles=16;}
   else if(op>=0xd0&&op<0xe0){acc_=op&15;zf_=acc_==0;sf_=true;next();}
   else if(op>=0xe0&&op<0xf0){stack_push(pc_+1);unsigned n=op&15;pc_=n*8+6+(n?0:0x80);cycles=16;}
   else if(op>=0xf0){unsigned b=op&3;if(op<0xf4){mem()&=uint8_t(~(1u<<b));sf_=true;}else if(op<0xf8){mem()|=1u<<b;sf_=true;}else if(op<0xfc)sf_=(acc_&(1u<<b))==0;else sf_=(mem()&(1u<<b))==0;next();}
   else switch(op){
   case 0x48:{unsigned k=arg>>4,y=arg&15;ram_[y]=k;sf_=true;next(2);cycles=16;break;}
   case 0x49:{unsigned k=arg>>4,y=arg&15;ram_[y]=(ram_[y]+k)&15;zf_=ram_[y]==0;sf_=ram_[y]>=k;next(2);cycles=16;break;}
   case 0x4a:set_io(arg&15,arg>>4);sf_=true;next(2);cycles=16;break;
   case 0x4b:{unsigned k=arg>>4,y=arg&15;cf_=k>=ram_[y];zf_=k==ram_[y];sf_=!zf_;next(2);cycles=16;break;}
   case 0x4c:{unsigned x=arg,t=hl_;hl_=(ram_[ram_bank_+x+1]<<4)|ram_[ram_bank_+x];ram_[ram_bank_+x]=t&15;ram_[ram_bank_+x+1]=t>>4;sf_=true;next(2);cycles=16;break;}
   case 0x4d:{unsigned s;pc_=stack_pop();s=STACK+(sp_<<2);cf_=ram_[s]&8;zf_=ram_[s]&4;sf_=ram_[s]&2;ei_=true;cycles=16;break;}
   case 0x4e:{unsigned x=arg;hl_=(ram_[ram_bank_+x+1]<<4)|ram_[ram_bank_+x];sf_=true;next(2);cycles=16;break;}
   case 0x4f:pc_=stack_pop();cycles=16;break;
   case 0x50:{bool c=(acc_&8)!=0;acc_=uint8_t(((acc_<<1)&14)|unsigned(cf_));cf_=c;zf_=!acc_;sf_=!c;next();break;}
   case 0x51:{bool c=(acc_&1)!=0;acc_=uint8_t((acc_>>1)|(unsigned(cf_)<<3));cf_=c;zf_=!acc_;sf_=!c;next();break;}
   case 0x52:sf_=cf_;cf_=true;next();break;case 0x53:sf_=!cf_;cf_=false;next();break;
   case 0x54:next();break;
   case 0x55:next(3);if(sf_)pc_=0x1000|(word((pc_-2)&0x1fff)&0xfff);sf_=true;cycles=24;break;
   case 0x56:next();break;
   case 0x57:next(3);if(sf_)pc_=word((pc_-2)&0x1fff)&0xfff;sf_=true;cycles=24;break;
   case 0x58:{uint8_t t=acc_;acc_=mem();mem()=t;zf_=!acc_;sf_=true;next();break;}
   case 0x59:mem()=acc_;sf_=true;next();break;case 0x5a:acc_=mem();zf_=!acc_;sf_=true;next();break;
   case 0x5b:sf_=zf_;next();break;case 0x5c:acc_=(acc_-1)&15;zf_=!acc_;sf_=acc_!=15;next();break;
   case 0x5d:mem()=(mem()-1)&15;zf_=!mem();sf_=mem()!=15;next();break;
   case 0x5e:acc_=(acc_+1)&15;zf_=!acc_;sf_=acc_!=0;next();break;
   case 0x5f:mem()=(mem()+1)&15;zf_=!mem();sf_=mem()!=0;next();break;
   case 0x60:{unsigned b=hl_&3,p=((hl_&12)>>2)+4;set_io(p,get_io(p)&~(1u<<b));sf_=true;next();cycles=16;break;}
   case 0x61:{unsigned b=hl_&3,p=((hl_&12)>>2)+4;sf_=(get_io(p)&(1u<<b))==0;next();cycles=16;break;}
   case 0x62:{unsigned b=hl_&3,p=((hl_&12)>>2)+4;set_io(p,get_io(p)|(1u<<b));sf_=true;next();cycles=16;break;}
   case 0x63:{unsigned mode=arg>>6,r=arg&0x3f;if(mode==1)ei_=true;else if(mode==2)ei_=false;il_&=r;sf_=true;next(2);cycles=16;break;}
   case 0x64:{uint8_t t=acc_;acc_=hl_&15;hl_=(hl_&0xf0)|t;zf_=!acc_;sf_=true;next();cycles=16;break;}
   case 0x65:acc_=rom(0x1000|dp_)&15;zf_=!acc_;sf_=true;next();cycles=16;break;
   case 0x66:{uint8_t t=acc_;acc_=hl_>>4;hl_=(hl_&15)|(t<<4);zf_=!acc_;sf_=true;next();cycles=16;break;}
   case 0x67:acc_=rom(0x1000|dp_)>>4;dp_=(dp_+1)&0xfff;zf_=!acc_;sf_=true;next();cycles=16;break;
   case 0x68:{uint8_t t=acc_;acc_=ram_[ram_bank_+arg];ram_[ram_bank_+arg]=t;zf_=!acc_;sf_=true;next(2);cycles=16;break;}
   case 0x69:{unsigned x=arg;if(x<0xf4)ram_[ram_bank_+x]=acc_;else if(x<=0xf6)timer_a_=(timer_a_&~(15u<<((x-0xf4)*4)))|(acc_<<((x-0xf4)*4));else if(x>=0xf8&&x<=0xfa)timer_b_=(timer_b_&~(15u<<((x-0xf8)*4)))|(acc_<<((x-0xf8)*4));else if(x>=0xfc&&x<=0xfe)dp_=(dp_&~(15u<<((x-0xfc)*4)))|(acc_<<((x-0xfc)*4));else if(x==0xff)sp_=acc_;sf_=true;next(2);cycles=16;break;}
   case 0x6a:{unsigned x=arg;if(x<0xf4)acc_=ram_[ram_bank_+x];else if(x<=0xf6)acc_=(timer_a_>>((x-0xf4)*4))&15;else if(x>=0xf8&&x<=0xfa)acc_=(timer_b_>>((x-0xf8)*4))&15;else if(x>=0xfc&&x<=0xfe)acc_=(dp_>>((x-0xfc)*4))&15;else if(x==0xff)acc_=sp_;zf_=!acc_;sf_=true;next(2);cycles=16;break;}
   case 0x6b:cf_=ram_[ram_bank_+arg]>=acc_;zf_=ram_[ram_bank_+arg]==acc_;sf_=!zf_;next(2);cycles=16;break;
   case 0x6c:{unsigned kind=arg>>6,b=(arg>>4)&3,y=arg&15;if(kind==0)sf_=(ram_[y]&(1u<<b))==0;else if(kind==1){ram_[y]|=1u<<b;sf_=true;}else if(kind==2)sf_=(ram_[y]&(1u<<b))!=0;else{ram_[y]&=uint8_t(~(1u<<b));sf_=true;}next(2);cycles=16;break;}
   case 0x6d:{unsigned kind=arg>>6,b=(arg>>4)&3,p=arg&15;if(kind==0)sf_=(get_io(p)&(1u<<b))==0;else if(kind==1){set_io(p,get_io(p)|(1u<<b));sf_=true;}else if(kind==2)sf_=(get_io(p)&(1u<<b))!=0;else{set_io(p,get_io(p)&~(1u<<b));sf_=true;}next(2);cycles=16;break;}
   case 0x6e:{unsigned kind=arg>>4,k=arg&15,r;switch(kind){case 1:r=((hl_&15)+k)&15;hl_=(hl_&0xf0)|r;zf_=!r;sf_=r>=k;break;case 3:zf_=k==(hl_&15);sf_=k>=(hl_&15);break;case 4:acc_|=k;zf_=!acc_;sf_=!zf_;break;case 5:acc_=(acc_+k)&15;zf_=!acc_;sf_=acc_>=k;break;case 6:acc_&=k;zf_=!acc_;sf_=!zf_;break;case 7:acc_=(k-acc_)&15;zf_=!acc_;sf_=k>=acc_;break;case 9:hl_=uint8_t(hl_+(k<<4));zf_=(hl_>>4)==0;sf_=(hl_>>4)>=k;break;case 11:zf_=(hl_>>4)==k;sf_=k>=(hl_>>4);break;case 12:mem()|=k;zf_=!mem();sf_=!zf_;break;case 13:mem()=(mem()+k)&15;zf_=!mem();sf_=mem()>=k;break;case 14:mem()&=k;zf_=!mem();sf_=!zf_;break;case 15:mem()=(k-mem())&15;zf_=!mem();sf_=k>=mem();break;default:break;}next(2);cycles=16;break;}
   case 0x6f:{unsigned kind=arg>>6,p=(kind==0||kind==2)?(arg&31):(arg&15);if(kind==0){set_io(p,acc_);sf_=true;}else if(kind==1){acc_=get_io(p);zf_=!acc_;sf_=!zf_;}else if(kind==2){set_io(p,mem());sf_=true;}else{mem()=get_io(p);sf_=mem()!=0;}next(2);cycles=16;break;}
   case 0x70:{unsigned oldcf=cf_,m=mem();acc_=(acc_+m+oldcf)&15;cf_=acc_<(m+oldcf);zf_=!acc_;sf_=!cf_;next();break;}
   case 0x71:acc_=(acc_+mem())&15;zf_=!acc_;sf_=acc_>=mem();next();break;
   case 0x72:{unsigned oldcf=cf_,m=mem();acc_=(m-acc_-(!oldcf))&15;cf_=m>=(acc_+(!oldcf));zf_=!acc_;sf_=cf_;next();break;}
   case 0x73:cf_=mem()>=acc_;zf_=mem()==acc_;sf_=!zf_;next();break;
   case 0x74:acc_=hl_&15;zf_=!acc_;sf_=true;next();break;
   case 0x75:{uint8_t t=acc_;acc_=imask_;imask_=t;sf_=true;next();break;}
   case 0x76:acc_=hl_>>4;zf_=!acc_;sf_=true;next();break;case 0x77:next();break;
   case 0x78:acc_|=mem();zf_=!acc_;sf_=!zf_;next();break;case 0x79:acc_^=mem();zf_=!acc_;sf_=!zf_;next();break;
   case 0x7a:next();break;case 0x7b:acc_&=mem();zf_=!acc_;sf_=!zf_;next();break;
   case 0x7c:hl_=(hl_&0xf0)|((hl_-1)&15);zf_=!(hl_&15);sf_=(hl_&15)!=15;next();break;
   case 0x7d:mem()=acc_;hl_=(hl_&0xf0)|((hl_-1)&15);zf_=!(hl_&15);sf_=(hl_&15)!=15;next();break;
   case 0x7e:hl_=(hl_&0xf0)|((hl_+1)&15);zf_=!(hl_&15);sf_=(hl_&15)!=0;next();break;
   case 0x7f:mem()=acc_;hl_=(hl_&0xf0)|((hl_+1)&15);zf_=!(hl_&15);sf_=(hl_&15)!=0;next();break;
   }
   ++instructions_;process_timers(cycles);return cycles;
}
