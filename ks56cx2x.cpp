#include "ks56cx2x.hpp"
#include <algorithm>
#include <cstring>

namespace{
constexpr unsigned basic_div[7]={4096,4096,512,512,128,128,32};
constexpr unsigned watch_div[2]={16384,128};
constexpr unsigned t0_div[8]={0,0,0,0,1024,256,64,16};
constexpr unsigned main_div[4]={64,16,8,4};
constexpr unsigned pc_normal=0,pc_halt=1,pc_stop=2;
}

uint8_t KS56CX2X::rom(unsigned a)const{return rom_.empty()?0:rom_[a%rom_.size()];}
uint16_t KS56CX2X::word(unsigned a)const{return uint16_t((rom(a)<<8)|rom(a+1));}
uint32_t KS56CX2X::bytes(unsigned a,unsigned n)const{uint32_t v=0;while(n--)v=(v<<8)|rom(a++);return v;}
unsigned KS56CX2X::opcode_size(uint8_t o)const{
   if(o>=0x40&&o<=0x47)return 2;if(o>=0x50&&o<=0x5f)return 2;
   switch(o){case 0x82:case 0x84:case 0x85:case 0x86:case 0x87:case 0x89:case 0x8b:case 0x8d:case 0x8f:
   case 0x92:case 0x93:case 0x94:case 0x95:case 0x96:case 0x97:case 0x99:case 0x9a:case 0x9b:case 0x9c:case 0x9d:case 0x9f:
   case 0xa2:case 0xa3:case 0xa4:case 0xa5:case 0xa6:case 0xa7:case 0xaa:case 0xac:case 0xae:
   case 0xb2:case 0xb3:case 0xb4:case 0xb5:case 0xb6:case 0xb7:case 0xb9:case 0xbc:case 0xbd:case 0xbe:case 0xbf:return 2;
   case 0xab:return 3;default:return 1;}
}
bool KS56CX2X::load(const void*d,std::size_t n,Profile profile){if(!d||n!=2048)return false;rom_.assign((const uint8_t*)d,(const uint8_t*)d+n);profile_=profile;reset();return true;}
void KS56CX2X::reset(){
   ram_.fill(0);pc_=sp_=cy_=mbe_=mbs_=rbe_=rbs_=sbs_=ist_=sk_=ime_=ips_=pcc_mode_=pcc_clock_=0;
   tmod2h_=tmod0_=0xff;wdtm_=bt_=btm_=scc_=wml_=wmh_=tm0l_=tm0h_=t0_=0;
   im0_=im1_=im2_=inta_=intc_=inte_=intf_=intg_=inth_=0;ports_={};input_buttons_.fill(false);port_latch_={};
   poga_=pogb_=pm3_=pm6_=0;sound_level_=0;basic_timer_=t0_timer_=watch_timer_=0;cpu_clock_div_=64;instructions_=0;go_vector(0);
}
void KS56CX2X::go_vector(unsigned a){uint16_t v=word(a);mbe_=(v>>15)&1;rbe_=(v>>14)&1;pc_=v&0xfff;}

uint8_t KS56CX2X::get_io(unsigned a){
   auto ext=[&](unsigned i,unsigned pull){return uint8_t((~ports_[i][0])&(ports_[i][1]|(pull?15:0))&15);};
   switch(a){
   case 0xf80:return sp_&15;case 0xf81:return sp_>>4;case 0xf82:return rbs_;case 0xf83:return mbs_;case 0xf84:return sbs_;
   case 0xf86:return bt_&15;case 0xf87:return bt_>>4;case 0xf88:return tmod2h_&15;case 0xf89:return tmod2h_>>4;
   case 0xf98:return wml_;case 0xf99:return wmh_;case 0xfa0:return tm0l_;case 0xfa1:return tm0h_;
   case 0xfa4:return t0_&15;case 0xfa5:return t0_>>4;case 0xfa6:return tmod0_&15;case 0xfa7:return tmod0_>>4;
   case 0xfb0:return uint8_t((ist_<<2)|(mbe_<<1)|rbe_);case 0xfb1:return uint8_t((cy_<<3)|sk_);
   case 0xfb2:return uint8_t((ime_<<3)|ips_);case 0xfb3:return uint8_t((pcc_mode_<<2)|pcc_clock_);
   case 0xfb4:return im0_;case 0xfb5:return im1_;case 0xfb6:return im2_;case 0xfb7:return scc_;
   case 0xfb8:return inta_;case 0xfba:return intc_;case 0xfbc:return inte_;case 0xfbd:return intf_;case 0xfbe:return intg_;case 0xfbf:return inth_;
   case 0xfdc:return poga_&15;case 0xfdd:return poga_>>4;case 0xfe8:return pm3_;case 0xfe9:return pm6_;
   case 0xff0:return ext(0,poga_&1);case 0xff1:return ext(1,poga_&2);case 0xff2:return ext(2,poga_&4);
   case 0xff3:return uint8_t((ext(3,poga_&8)&~pm3_)|(port_latch_[1]&pm3_));case 0xff5:return ext(4,0);
   case 0xff6:return uint8_t((ext(5,poga_&0x32)&~pm6_)|(port_latch_[3]&pm6_));
   case 0xff8:return ext(6,pogb_&1);case 0xff9:return ext(7,pogb_&2);default:return 0;}
}
void KS56CX2X::set_io(unsigned a,uint8_t v){v&=15;switch(a){
   case 0xf80:sp_=(sp_&0xf0)|v;break;case 0xf81:sp_=(sp_&15)|(v<<4);break;case 0xf84:sbs_=v;break;
   case 0xf85:btm_=v&7;if(v&8){inta_&=~1;bt_=0;}break;case 0xf88:tmod2h_=(tmod2h_&0xf0)|v;break;case 0xf89:tmod2h_=(tmod2h_&15)|(v<<4);break;
   case 0xf8b:wdtm_=(v>>3)&1;break;case 0xf98:wml_=v;break;case 0xf99:wmh_=v&11;break;
   case 0xfa0:tm0l_=v&12;if(v&8){inte_&=~1;t0_=0;}break;case 0xfa1:tm0h_=v&7;break;
   case 0xfa6:tmod0_=(tmod0_&0xf0)|v;break;case 0xfa7:tmod0_=(tmod0_&15)|(v<<4);break;
   case 0xfb0:ist_=(v>>2)&3;mbe_=(v>>1)&1;rbe_=v&1;break;case 0xfb1:cy_=(v>>3)&1;sk_=v&7;break;
   case 0xfb2:ime_=(v>>3)&1;ips_=v&7;break;case 0xfb3:pcc_mode_=(v>>2)&3;pcc_clock_=v&3;update_clock_div();break;
   case 0xfb4:im0_=v;break;case 0xfb5:im1_=v&1;break;case 0xfb6:im2_=v&3;break;case 0xfb7:scc_=v&9;update_clock_div();break;
   case 0xfb8:inta_=v;break;case 0xfba:intc_=v;break;case 0xfbc:inte_=v;break;case 0xfbd:intf_=v;break;case 0xfbe:intg_=v;break;case 0xfbf:inth_=v;break;
   case 0xfdc:poga_=(poga_&0xf0)|v;break;case 0xfdd:poga_=(poga_&15)|(v<<4);break;case 0xfe8:pm3_=v;break;case 0xfe9:pm6_=v;break;
   case 0xff2:port_latch_[0]=v;break;case 0xff3:port_latch_[1]=v;sound_level_=(v&8)?1:0;break;
   case 0xff5:port_latch_[2]=v;break;case 0xff6:port_latch_[3]=v;break;case 0xff8:port_latch_[4]=v;break;case 0xff9:port_latch_[5]=v;break;default:break;}}

uint8_t KS56CX2X::get_mem(unsigned a){unsigned mb=15;if(mbe_||a<0x80)mb=mbe_*mbs_;return mb==15?get_io(0xf00+a):ram_[(mb<<8)+a];}
void KS56CX2X::set_mem(unsigned a,uint8_t v){unsigned mb=15;if(mbe_||a<0x80)mb=mbe_*mbs_;if(mb==15)set_io(0xf00+a,v);else ram_[(mb<<8)+a]=v&15;}
uint8_t KS56CX2X::get_ahl(){unsigned mb=mbe_*mbs_,a=(mb<<8)+get_rp(HL);return mb==15?get_io(a):ram_[a];}
uint8_t KS56CX2X::get_ahl_byte(){unsigned mb=mbe_*mbs_,a=(mb<<8)+get_rp(HL);return mb==15?uint8_t((get_io(a+1)<<4)|get_io(a)):uint8_t((ram_[a+1]<<4)|ram_[a]);}
void KS56CX2X::set_ahl(uint8_t v){unsigned mb=mbe_*mbs_,a=(mb<<8)+get_rp(HL);if(mb==15)set_io(a,v);else ram_[a]=v&15;}
void KS56CX2X::set_ahl_byte(uint8_t v){unsigned mb=mbe_*mbs_,a=(mb<<8)+get_rp(HL);if(mb==15){set_io(a,v);set_io(a+1,v>>4);}else{ram_[a]=v&15;ram_[a+1]=v>>4;}}
uint8_t KS56CX2X::get_hmem(uint32_t o){unsigned a=(get_reg(H)<<4)|(o&15),mb=mbe_*mbs_;return mb==15?get_io(0xf00+a):ram_[(mb<<8)+a];}
void KS56CX2X::set_hmem(uint32_t o,uint8_t v){unsigned a=(get_reg(H)<<4)|(o&15),mb=mbe_*mbs_;if(mb==15)set_io(0xf00+a,v);else ram_[(mb<<8)+a]=v&15;}
uint8_t KS56CX2X::get_pmeml(uint32_t o){return get_io(0xfc0|((o&15)<<2)|(get_reg(L)>>2));}
void KS56CX2X::set_pmeml(uint32_t o,uint8_t v){set_io(0xfc0|((o&15)<<2)|(get_reg(L)>>2),v);}
uint8_t KS56CX2X::get_fmem(uint32_t o){return get_io(0xfb0|(o&0x4f));}
void KS56CX2X::set_fmem(uint32_t o,uint8_t v){set_io(0xfb0|(o&0x4f),v);}
uint8_t KS56CX2X::get_reg(unsigned r)const{return ram_[rbe_*rbs_*8+r];}
void KS56CX2X::set_reg(unsigned r,uint8_t v){ram_[rbe_*rbs_*8+r]=v&15;}
uint8_t KS56CX2X::get_rp(unsigned r)const{unsigned a=(rbe_*rbs_*8+(r&6))^((r&1)<<3);return uint8_t((ram_[a+1]<<4)|ram_[a]);}
void KS56CX2X::set_rp(unsigned r,uint8_t v){unsigned a=(rbe_*rbs_*8+(r&6))^((r&1)<<3);ram_[a]=v&15;ram_[a+1]=v>>4;}
void KS56CX2X::stack_push(uint8_t v){sp_=(sp_-1)&255;ram_[((sbs_&1)<<8)+sp_]=v&15;}
void KS56CX2X::stack_push_byte(uint8_t v){stack_push(v>>4);stack_push(v);}
uint8_t KS56CX2X::stack_pop(){unsigned a=((sbs_&1)<<8)+sp_;sp_=(sp_+1)&255;return ram_[a];}
uint8_t KS56CX2X::stack_pop_byte(){uint8_t l=stack_pop(),h=stack_pop();return uint8_t((h<<4)|l);}
unsigned KS56CX2X::skip_next(){unsigned n=opcode_size(rom(pc_));pc_+=n;return n<3?1:2;}

void KS56CX2X::update_clock_div(){cpu_clock_div_=(scc_&1)?uint32_t(1000000.0/32768.0):main_div[pcc_clock_];}
void KS56CX2X::process_timers(unsigned c){
   basic_timer_-=c;while(basic_timer_<=0){basic_timer_+=basic_div[btm_];bt_++;if(!bt_&&!wdtm_)inta_|=1;}
   if(t0_div[tm0h_]&&(tm0l_&4)){t0_timer_-=c;while(t0_timer_<=0){t0_timer_+=t0_div[tm0h_];t0_++;if(t0_==tmod0_){inte_|=1;t0_=0;}}}
   if(wml_&4){watch_timer_-=c;while(watch_timer_<=0){watch_timer_+=int64_t(watch_div[(wml_&2)?1:0])*((wml_&1)?(1000000.0/32768.0):128);intc_|=1;}}else watch_timer_=0;
}
unsigned KS56CX2X::interrupt_vector()const{unsigned q=0;auto pick=[&](bool c,unsigned v){if(c){if(v==ips_&&ist_<=1)return v;if(!ist_)q=v;}return 0u;};
   unsigned r;if((r=pick((inte_&2)&&(inte_&1),5)))return r;if((r=pick((intg_&8)&&(intg_&4),3)))return r;
   if((r=pick((intg_&2)&&(intg_&1),2)))return r;if((r=pick((inta_&8)&&(inta_&4),1)))return r;if((r=pick((inta_&2)&&(inta_&1),1)))return r;return q;}
void KS56CX2X::interrupt(unsigned q){stack_push((cy_<<3)|sk_);stack_push((ist_<<2)|(mbe_<<1)|rbe_);stack_push((pc_>>4)&15);stack_push(pc_&15);stack_push((mbe_<<3)|(rbe_<<2)|((pc_>>12)&1));stack_push((pc_>>8)&15);go_vector(q<<1);ist_++;
   if(q==1){if(!(inta_&2))inta_&=~4;else if(!(inta_&8))inta_&=~1;}else if(q==5)inte_&=~1;else if(q==2)intg_&=~1;else if(q==3)intg_&=~4;}

void KS56CX2X::restore_pc(){pc_=stack_pop()<<8;uint8_t v=stack_pop();pc_|=(v<<12)&0x1000;mbe_=v>>3;rbe_=(v>>2)&1;pc_|=stack_pop();pc_|=stack_pop()<<4;pc_&=0xfff;}
unsigned KS56CX2X::execute(uint8_t o,uint32_t code){
   if(o<0x10){pc_+=o;return 8;}
   if(o<0x40){unsigned a=code<<1,b=rom(a),n=opcode_size(b),t=1;if(n==1){if(!(b&0xc0)){pc_=word(a)&0xfff;return 3;}uint32_t c=b;t+=execute(b,c);t+=execute(rom(a+1),c);}else t+=execute(b,bytes(a,n));return t;}
   if(o<0x48){stack_push((pc_>>4)&15);stack_push(pc_&15);stack_push((mbe_<<3)|(rbe_<<2)|((pc_>>12)&3));stack_push((pc_>>8)&15);pc_=code&0xfff;return 2;}
   if(o<0x50){if(o&1)stack_push_byte(get_rp(o&6));else set_rp(o&6,stack_pop_byte());return 1;}
   if(o<0x60){pc_=((pc_&0xf000)|(code&0xfff))&0xfff;return 2;}
   if(o<0x70){unsigned n=get_reg(A)+(o&15);set_reg(A,n);return n>15?1+skip_next():1;}
   if(o<0x80){set_reg(A,o&15);unsigned t=1,n=rom(pc_);while((n>>4)==7||n==0x89){t++;pc_+=opcode_size(n);n=rom(pc_);}return t;}
   auto mem_bit=[&](unsigned mode){unsigned d=code&255,b=(code>>12)&3,v=get_mem(d),m=1u<<b;if(mode==0)set_mem(d,v&~m);else if(mode==1)set_mem(d,v|m);else if((mode==2&&!(v&m))||(mode==3&&(v&m)))return 2+skip_next();return 2u;};
   switch(o){
   case 0x80:return get_reg(A)==get_ahl()?1+skip_next():1;
   case 0x82:{unsigned d=code&255,v=(get_mem(d)+1)&15;set_mem(d,v);return v?2:2+skip_next();}
   case 0x84:case 0x94:case 0xa4:case 0xb4:return mem_bit(0);
   case 0x85:case 0x95:case 0xa5:case 0xb5:return mem_bit(1);
   case 0x86:case 0x96:case 0xa6:case 0xb6:return mem_bit(2);
   case 0x87:case 0x97:case 0xa7:case 0xb7:return mem_bit(3);
   case 0x89:{set_rp(XA,code);unsigned t=2,n=rom(pc_);while((n>>4)==7||n==0x89){t++;pc_+=opcode_size(n);n=rom(pc_);}return t;}
   case 0x8a:case 0x8c:case 0x8e:{unsigned r=o&6,v=(get_rp(r)+1)&255;set_rp(r,v);return v?1:1+skip_next();}
   case 0x8b:{set_rp(HL,code);unsigned t=2,n=rom(pc_);while(n==0x8b){t++;pc_+=opcode_size(n);n=rom(pc_);}return t;}
   case 0x8d:set_rp(DE,code);return 2;case 0x8f:set_rp(BC,code);return 2;
   case 0x90:set_reg(A,get_reg(A)&get_ahl());return 1;
   case 0x92:set_mem(code&255,get_reg(A));set_mem((code&255)+1,get_reg(X));return 2;
   case 0x93:set_mem(code&255,get_reg(A));return 2;
   case 0x98:{unsigned c=get_reg(A)&1;set_reg(A,(get_reg(A)>>1)|(cy_<<3));cy_=c;return 1;}
   case 0x99:return exec99(code);case 0x9a:{unsigned r=code&7;if(code&8){set_reg(r,(code>>4)&15);return 2;}return get_reg(r)==((code>>4)&15)?2+skip_next():2;}
   case 0x9b:case 0x9c:case 0x9d:case 0x9f:case 0xac:case 0xae:case 0xbc:case 0xbd:case 0xbe:case 0xbf:return exec_bit_group(o,code);
   case 0xa0:set_reg(A,get_reg(A)|get_ahl());return 1;
   case 0xa2:{unsigned d=code&0xfe;set_reg(A,get_mem(d));set_reg(X,get_mem(d+1));return 2;}
   case 0xa3:set_reg(A,get_mem(code&255));return 2;
   case 0xa8:{int n=int(get_reg(A))-get_ahl();set_reg(A,n);return n<0?1+skip_next():1;}
   case 0xa9:{unsigned n=get_reg(A)+get_ahl()+cy_;set_reg(A,n);cy_=n>15;uint8_t q=rom(pc_);if((q>>4)==6){if(cy_)return 1+skip_next();pc_++;set_reg(A,get_reg(A)+(q&15));return 2;}return 1;}
   case 0xaa:return execaa(code);
   case 0xab:if(code&0x4000){stack_push((pc_>>4)&15);stack_push(pc_&15);stack_push((mbe_<<3)|(rbe_<<2)|((pc_>>12)&3));stack_push((pc_>>8)&15);}pc_=code&0xfff;return (code&0x4000)?3:1;
   case 0xb0:set_reg(A,get_reg(A)^get_ahl());return 1;
   case 0xb2:{unsigned d=code&0xfe,a=get_reg(A),x=get_reg(X);set_reg(A,get_mem(d));set_reg(X,get_mem(d+1));set_mem(d,a);set_mem(d+1,x);return 2;}
   case 0xb3:{unsigned d=code&255,a=get_reg(A);set_reg(A,get_mem(d));set_mem(d,a);return 2;}
   case 0xb8:{int n=int(get_reg(A))-get_ahl()-cy_;set_reg(A,n);cy_=n<0;uint8_t q=rom(pc_);if((q>>4)==6){if(cy_){pc_++;set_reg(A,get_reg(A)+(q&15));return 2;}return 1+skip_next();}return 1;}
   case 0xb9:{unsigned n=get_rp(XA)+(code&255);set_rp(XA,n);return n>255?2+skip_next():2;}
   default:break;}
   if(o>=0xc0&&o<=0xc7){unsigned r=o&7,v=(get_reg(r)+1)&15;set_reg(r,v);return v?1:1+skip_next();}
   if(o>=0xc8&&o<=0xcf){unsigned r=o&7,v=(get_reg(r)-1)&15;set_reg(r,v);return v==15?1+skip_next():1;}
   switch(o){
   case 0xd0:set_rp(XA,rom((pc_&0xff00)|get_rp(XA)));return 3;case 0xd1:set_rp(XA,rom((get_rp(BC)<<8)|get_rp(XA)));return 3;
   case 0xd2:{unsigned n=get_reg(A)+get_ahl();set_reg(A,n);return n>15?1+skip_next():1;}
   case 0xd4:set_rp(XA,rom((pc_&0xff00)|get_rp(DE)));return 3;case 0xd5:set_rp(XA,rom((get_rp(BC)<<8)|get_rp(DE)));return 3;
   case 0xd6:cy_=!cy_;return 1;case 0xd7:return cy_?1+skip_next():1;default:break;}
   if(o>=0xd8&&o<=0xdf){unsigned r=o&7,a=get_reg(A);set_reg(A,get_reg(r));set_reg(r,a);return 1;}
   switch(o){
   case 0xe0:restore_pc();return 3+skip_next();case 0xe1:set_reg(A,get_ahl());return 1;
   case 0xe2:{set_reg(A,get_ahl());unsigned v=(get_reg(L)+1)&15;set_reg(L,v);return v?1:2+skip_next();}
   case 0xe3:{set_reg(A,get_ahl());unsigned v=(get_reg(L)-1)&15;set_reg(L,v);return v==15?2+skip_next():1;}
   case 0xe4:set_reg(A,ram_[get_rp(DE)]);return 1;case 0xe5:set_reg(A,ram_[(get_reg(D)<<4)|get_reg(L)]);return 1;
   case 0xe6:cy_=0;return 1;case 0xe7:cy_=1;return 1;case 0xe8:set_ahl(get_reg(A));return 1;
   case 0xe9:{unsigned a=get_reg(A);set_reg(A,get_ahl());set_ahl(a);return 1;}
   case 0xea:{unsigned a=get_reg(A);set_reg(A,get_ahl());set_ahl(a);unsigned v=(get_reg(L)+1)&15;set_reg(L,v);return v?1:2+skip_next();}
   case 0xeb:{unsigned a=get_reg(A);set_reg(A,get_ahl());set_ahl(a);unsigned v=(get_reg(L)-1)&15;set_reg(L,v);return v==15?2+skip_next():1;}
   case 0xec:{unsigned d=get_rp(DE),a=get_reg(A);set_reg(A,ram_[d]);ram_[d]=a;return 1;}
   case 0xed:{unsigned d=(get_reg(D)<<4)|get_reg(L),a=get_reg(A);set_reg(A,ram_[d]);ram_[d]=a;return 1;}
   case 0xee:restore_pc();return 3;case 0xef:{restore_pc();uint8_t v=stack_pop();ist_=v>>2;mbe_=(v>>1)&1;rbe_=v&1;v=stack_pop();cy_=(v>>3)&1;sk_=v&7;return 3;}default:break;}
   if(o>=0xf0){pc_-=16-(o&15);return 8;}return 0;
}

unsigned KS56CX2X::exec99(uint32_t o){unsigned q=o&0x7f;
   if(q==0){pc_=((pc_&0xff00)|(get_reg(X)<<4)|get_reg(A))&0xfff;return 3;}if(q==1){pc_=((get_reg(B)<<12)|(get_reg(C)<<8)|(get_reg(X)<<4)|get_reg(A))&0xfff;return 3;}
   if(q==2){unsigned v=(get_ahl()+1)&15;set_ahl(v);return v?2:2+skip_next();}if(q==4){pc_=((pc_&0xff00)|(get_reg(D)<<4)|get_reg(E))&0xfff;return 3;}
   if(q==5){pc_=((get_reg(B)<<12)|(get_reg(C)<<8)|(get_reg(D)<<4)|get_reg(E))&0xfff;return 3;}if(q==6){rbs_=stack_pop();mbs_=stack_pop();return 2;}
   if(q==7){stack_push(mbs_);stack_push(rbs_);return 2;}if(q<16)return get_reg(A)==get_reg(q&7)?2+skip_next():2;
   if(q<32){mbs_=q&15;return 2;}if(q<36){rbs_=q&3;return 2;}if(q>=48&&q<64){set_reg(A,get_reg(A)&q);return 2;}
   if(q>=64&&q<80){set_reg(A,get_reg(A)|q);return 2;}if(q>=80&&q<96){set_reg(A,get_reg(A)^q);return 2;}
   if(q>=96&&q<112)return get_ahl()==(q&15)?2+skip_next():2;if(q>=112&&q<120){set_reg(q&7,get_reg(A));return 2;}
   if(q>=120){set_reg(A,get_reg(q&7));return 2;}return 0;
}
unsigned KS56CX2X::execaa(uint32_t o){unsigned q=o&255,r=q&7,xa=get_rp(XA),rp=get_rp(r);switch(q){
   case 0x10:set_ahl_byte(xa);return 2;case 0x11:set_rp(XA,get_ahl_byte());set_ahl_byte(xa);return 2;
   case 0x18:set_rp(XA,get_ahl_byte());return 2;case 0x19:return xa==get_ahl_byte()?2+skip_next():2;default:break;}
   if(q>=0x40&&q<0x48){set_rp(XA,rp);set_rp(r,xa);return 2;}if(q>=0x48&&q<0x50)return xa==rp?2+skip_next():2;
   if(q>=0x50&&q<0x58){set_rp(r,xa);return 2;}if(q>=0x58&&q<0x60){set_rp(XA,rp);return 2;}
   if(q>=0x68&&q<0x70){unsigned v=(rp-1)&255;set_rp(r,v);return v==255?2+skip_next():2;}
   if(q>=0x90&&q<0x98){set_rp(r,rp&xa);return 2;}if(q>=0x98&&q<0xa0){set_rp(XA,rp&xa);return 2;}
   if(q>=0xa0&&q<0xa8){set_rp(r,rp|xa);return 2;}if(q>=0xa8&&q<0xb0){set_rp(XA,rp|xa);return 2;}
   if(q>=0xb0&&q<0xb8){set_rp(r,rp^xa);return 2;}if(q>=0xb8&&q<0xc0){set_rp(XA,rp^xa);return 2;}
   if(q>=0xc0&&q<0xc8){unsigned v=rp+xa;set_rp(r,v);return v>255?2+skip_next():2;}if(q>=0xc8&&q<0xd0){unsigned v=rp+xa;set_rp(XA,v);return v>255?2+skip_next():2;}
   if(q>=0xd0&&q<0xd8){unsigned v=rp+xa+cy_;set_rp(r,v);cy_=v>255;return 2;}if(q>=0xd8&&q<0xe0){unsigned v=rp+xa+cy_;set_rp(XA,v);cy_=v>255;return 2;}
   if(q>=0xe0&&q<0xe8){int v=int(rp)-xa;set_rp(r,v);return v<0?2+skip_next():2;}if(q>=0xe8&&q<0xf0){int v=int(xa)-rp;set_rp(XA,v);return v<0?2+skip_next():2;}
   if(q>=0xf0&&q<0xf8){int v=int(rp)-xa-cy_;set_rp(r,v);cy_=v<0;return 2;}if(q>=0xf8){int v=int(xa)-rp-cy_;set_rp(XA,v);cy_=v<0;return 2;}return 0;
}
unsigned KS56CX2X::exec_bit_group(uint8_t op,uint32_t o){unsigned group=(o>>6)&3,b,mem;auto get=[&](){if(group==0){b=(o>>4)&3;return unsigned(get_hmem(o));}if(group==1){b=get_reg(L)&3;return unsigned(get_pmeml(o));}b=(o>>4)&3;return unsigned(get_fmem(o));};auto set=[&](unsigned v){if(group==0)set_hmem(o,v);else if(group==1)set_pmeml(o,v);else set_fmem(o,v);};mem=get();unsigned bit=(mem>>b)&1;
   if(op==0x9b){if(cy_)set(mem|(1u<<b));else set(mem&~(1u<<b));return 2;}
   if(op==0x9c){set(mem&~(1u<<b));return 2;}if(op==0x9d){set(mem|(1u<<b));return 2;}
   if(op==0x9f){if(bit){set(mem&~(1u<<b));return 2+skip_next();}return 2;}
   if(op==0xac)cy_&=bit;else if(op==0xae)cy_|=bit;else if(op==0xbc)cy_^=bit;else if(op==0xbd)cy_=bit;
   else if(op==0xbe)return !bit?2+skip_next():2;else if(op==0xbf)return bit?2+skip_next():2;return 2;
}

unsigned KS56CX2X::clock(){unsigned c=cpu_clock_div_;if(pcc_mode_==pc_normal){uint8_t o=rom(pc_);unsigned n=opcode_size(o);uint32_t code=bytes(pc_,n);pc_+=n;c*=execute(o,code);instructions_++;}
   else if(((intc_&3)==3)||((inth_&3)==3)||((intg_&3)==3&&(im0_&4))||((intg_&12)==12)||((inte_&3)==3)||((inta_&3)==3))pcc_mode_=pc_normal;
   if(pcc_mode_!=pc_stop){process_timers(c);}if(ime_){unsigned q=interrupt_vector();if(q)interrupt(q);}return c;
}

void KS56CX2X::port_input(unsigned idx,unsigned mask,bool down){uint8_t prev=get_io(idx==1?0xff1:0xff6);auto&p=ports_[idx];p[0]&=~mask;p[1]&=~mask;if(down)p[0]|=mask;uint8_t now=get_io(idx==1?0xff1:0xff6);
   if(idx==1&&prev!=now){if((mask&2)&&(im1_!=((now>>1)&1)))intg_|=4;if((mask&1)&&im0_!=3&&(((im0_&1)!=unsigned(!down))||(im0_&2)))intg_|=1;}
   if(idx==5&&down&&prev!=now&&!(pm6_&mask)&&(im2_==3||(im2_==2&&(mask&12))))inth_|=1;
}
void KS56CX2X::set_button(unsigned b,bool down){
   static const unsigned ga878_idx[7]={1,1,5,5,5,5,1},ga878_mask[7]={8,4,1,2,4,2,8};
   static const unsigned micon_idx[7]={1,1,1,5,1,5,5},micon_mask[7]={1,4,2,4,8,2,1};
   if(b==7){if(down)reset();return;}if(b>=7)return;input_buttons_[b]=down;
   const unsigned*idx=profile_==Profile::MiconKC32?micon_idx:ga878_idx;
   const unsigned*mask=profile_==Profile::MiconKC32?micon_mask:ga878_mask;
   bool combined=false;for(unsigned i=0;i<7;i++)if(idx[i]==idx[b]&&mask[i]==mask[b])combined|=input_buttons_[i];port_input(idx[b],mask[b],combined);
}
bool KS56CX2X::wake_from_deep_sleep(){
   if(pcc_mode_!=pc_stop)return false;
   if(profile_==Profile::MiconKC32){reset();set_button(5,true);}
   else set_button(4,true);
   return true;
}

void KS56CX2X::save(Snapshot&s)const{s.ram=ram_;s.pc=pc_;s.instructions=instructions_;s.basic_timer=basic_timer_;s.t0_timer=t0_timer_;s.watch_timer=watch_timer_;s.cpu_clock_div=cpu_clock_div_;s.sp=sp_;
   uint8_t a[]={cy_,mbe_,mbs_,rbe_,rbs_,sbs_,ist_,sk_,ime_,ips_,pcc_mode_,pcc_clock_,tmod2h_,wdtm_,bt_,btm_,scc_,wml_,wmh_,tm0l_,tm0h_,t0_,tmod0_,im0_,im1_,im2_,inta_,intc_,inte_,intf_,intg_,inth_};
   s.cy=a[0];s.mbe=a[1];s.mbs=a[2];s.rbe=a[3];s.rbs=a[4];s.sbs=a[5];s.ist=a[6];s.sk=a[7];s.ime=a[8];s.ips=a[9];s.pcc_mode=a[10];s.pcc_clock=a[11];s.tmod2h=a[12];s.wdtm=a[13];s.bt=a[14];s.btm=a[15];s.scc=a[16];s.wml=a[17];s.wmh=a[18];s.tm0l=a[19];s.tm0h=a[20];s.t0=a[21];s.tmod0=a[22];s.im0=a[23];s.im1=a[24];s.im2=a[25];s.inta=a[26];s.intc=a[27];s.inte=a[28];s.intf=a[29];s.intg=a[30];s.inth=a[31];
   for(unsigned i=0;i<8;i++)for(unsigned j=0;j<2;j++)s.ports[i][j]=ports_[i][j];std::copy(port_latch_.begin(),port_latch_.end(),s.port_latch);s.poga=poga_;s.pogb=pogb_;s.pm3=pm3_;s.pm6=pm6_;s.sound_level=uint8_t(sound_level_);}
void KS56CX2X::restore(const Snapshot&s){ram_=s.ram;pc_=s.pc;instructions_=s.instructions;basic_timer_=s.basic_timer;t0_timer_=s.t0_timer;watch_timer_=s.watch_timer;cpu_clock_div_=s.cpu_clock_div;sp_=s.sp;
   cy_=s.cy;mbe_=s.mbe;mbs_=s.mbs;rbe_=s.rbe;rbs_=s.rbs;sbs_=s.sbs;ist_=s.ist;sk_=s.sk;ime_=s.ime;ips_=s.ips;pcc_mode_=s.pcc_mode;pcc_clock_=s.pcc_clock;tmod2h_=s.tmod2h;wdtm_=s.wdtm;bt_=s.bt;btm_=s.btm;scc_=s.scc;wml_=s.wml;wmh_=s.wmh;tm0l_=s.tm0l;tm0h_=s.tm0h;t0_=s.t0;tmod0_=s.tmod0;im0_=s.im0;im1_=s.im1;im2_=s.im2;inta_=s.inta;intc_=s.intc;inte_=s.inte;intf_=s.intf;intg_=s.intg;inth_=s.inth;
   for(unsigned i=0;i<8;i++)for(unsigned j=0;j<2;j++)ports_[i][j]=s.ports[i][j];std::copy(s.port_latch,s.port_latch+6,port_latch_.begin());poga_=s.poga;pogb_=s.pogb;pm3_=s.pm3;pm6_=s.pm6;sound_level_=s.sound_level;input_buttons_.fill(false);}
