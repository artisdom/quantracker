/*
 Copyright (c) 2013 -2015 Andy Little 

 With Grateful Acknowledgements to the prior work of:
   Sami Korhonen(Openpilot.org)
   taulabs ( taulabs.com) 
   brainFPV ( brainfpv.com)
   Thomas Oldbury (super-osd.com)

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program. If not, see <http://www.gnu.org/licenses/>
*/

#include <stm32f4xx.h>
#include <quan/time.hpp>
#include <quan/frequency.hpp>
#include <quan/stm32/get_module_bus_frequency.hpp>
#include <quan/stm32/tim/temp_reg.hpp>
#include "video_cfg.hpp"
#include "osd_state.hpp"

// row line_counter on TIM3 (16 bit)
// if ! defined QUAN_OSD_SOFTWARE_SYNCSEP
// vsync on TIM3_CH1 -> TI used as trigger ITR

// hsync on TIM3_ETR -> ETR first edge used to count rows
// "External clock mode 2 + trigger mode"

// setup first with first telem row to give compare interrupt
// in that interrupt do whatever to start telem and set compare
// for end of telem rows
// in end of telem rows set up for start of osd output
// want to know the first telem row from hsync
// and last telem row
// transmit or receive ( or both for test!)
// could add compare 3 and 4 outputs
// compare 3 start of telem
// compare 4 end of telem
// using LM1881
/*
  First line is s6 during which linecounter is initialised to 0
  so when linecounter ==1 line == 7
  linecounter_line = actual_line - 6
*/
//##############  TODO  #########################
// Renumber using official line numbers of first field
// init count to value
// ##########for ntsc the last should be s15 so total == 9 lines of tele ###
// Think  m_begin can be 1
uint16_t video_cfg::rows::telem::m_begin = 3;// s9
uint16_t video_cfg::rows::telem::m_end = 16;// s23
//#############################################################
// make sure first active row is x2 more than last telem row
// dont think need to change for interlaced/ non interlaced
// except that on stm32f407 interlaced doesnt have
// enough memory for full display
// (actually 427 doesnt quite either)
// ################TODO  SORT FOR NTSC ########################
#if defined (QUAN_DISPLAY_INTERLACED)
// this is simply as there aint enough ram on the stm32f407
uint16_t video_cfg::rows::osd::m_begin = 132;
uint16_t video_cfg::rows::osd::m_end_pal = 480;
uint16_t video_cfg::rows::osd::m_end_ntsc = 480;
#else
uint16_t video_cfg::rows::osd::m_begin = 34;  // --> 34/2 +6 --> s23
// BALDLY just reduce this to 500 for NTSC?
// values are x2 ( from interlace) so /2 for count in frame
uint16_t video_cfg::rows::osd::m_end_pal = 600;     // --> 600/ 2 + 6 --> s306
uint16_t video_cfg::rows::osd::m_end_ntsc = 500;
#endif
//###############################################################
video_cfg::rows::mode video_cfg::rows::m_cur_mode = mode::idle;
bool video_cfg::rows::m_cur_row_odd = true;

// on first edge of hsync 
// start of first telem row
void video_cfg::rows::telem::begin()
{
   m_cur_mode = mode::telemetry;
   video_cfg::columns::telem::enable();
}

// on first edge of hsync
// start of one after last telem row
void video_cfg::rows::telem::end()
{
   video_cfg::columns::telem::disable();
   m_cur_mode = mode::idle;
}

// on first edge of hsync
// start of first osd row
void video_cfg::rows::osd::begin()
{
  m_cur_mode = mode::osd;
  video_cfg::columns::osd::enable();
}

#if defined QUAN_OSD_SOFTWARE_SYNCSEP
namespace detail{
   void sync_sep_enable();
}
#endif
// at end of this frame half ( odd or even)
// reset the hsync count for next
void video_cfg::rows::osd::end()
{
   video_cfg::columns::osd::disable();
   m_cur_mode = mode::idle;
   // clear the row line_counter ready for
  // counting rows of next frame half
  // line_counter::get()->cnt = 0;
   if( osd_state::get() == osd_state::external_video){
      video_cfg::rows::line_counter::get()->cr1.bb_clearbit<0>() ;// CEN
#if defined QUAN_OSD_SOFTWARE_SYNCSEP
      detail::sync_sep_enable();
#endif
   }
}

void video_cfg::rows::takedown()
{
    NVIC_DisableIRQ (TIM3_IRQn);
    m_cur_mode = mode::idle;
    m_cur_row_odd = true;
}

namespace {

   void video_cfg_rows_external_setup()
   {
      typedef video_cfg::rows::line_counter line_counter ;
   //   m_cur_mode = mode::idle;
   //   m_cur_row_odd = true;
      quan::stm32::module_enable<line_counter>();
      quan::stm32::module_reset<line_counter>();

      {
         quan::stm32::tim::cr1_t cr1 = line_counter::get()->cr1.get();
         cr1.opm = true; // one pulse mode
         line_counter::get()->cr1 .set (cr1.value);
      }
      {
         quan::stm32::tim::cr2_t cr2 = line_counter::get()->cr2.get();
         cr2.ti1s = false;
         line_counter::get()->cr2 .set (cr2.value);
      }


      {
         quan::stm32::tim::smcr_t smcr = line_counter::get()->smcr.get();
#if !defined QUAN_AERFLITE_BOARD
   // could be the way to disable LineCounter clock
         smcr.ece = true;  // external clock on TIM3_ETR
   // The source for the clock to count lines
   #if ((QUAN_OSD_BOARD_TYPE == 1) || (QUAN_OSD_BOARD_TYPE == 3) || (QUAN_OSD_BOARD_TYPE == 4) )
         smcr.etp = true;  // external clock TIM3_ETR falling edge ( first edge)
   #else
       #if (QUAN_OSD_BOARD_TYPE == 2)
         smcr.etp = false;  // external clock TIM3_ETR rising edge ( first edge)
       #else
         #error no board type defined
       #endif
   #endif
         smcr.etps = 0b00; // no prescaler on trigger
         smcr.etf = 0b000; // no external trigger filter
   #if !defined QUAN_OSD_SOFTWARE_SYNCSEP
        // with no software sync sep then
         // line_counter enabled by VSYNC
         smcr.sms = 0b110; // slave mode trigger
         smcr.ts  = 0b101; // trigger source input is TI1 (TIM3_CH1)
   #endif
#else
// aerflite
         smcr.etp = false; // (actuall dont care)
         smcr.ece = false; // Dont want external trigger
         smcr.etps = 0b00;
         smcr.etf  = 0b0000;
         smcr.ts  = 0b101;  // trigger source is TI1FP1 (TIM3_CH1)
         smcr.sms = 0b111;  // External clock mode

#endif
         line_counter::get()->smcr.set (smcr.value);
      }
      {
         quan::stm32::tim::ccmr1_t ccmr1 = line_counter::get()->ccmr1.get();
   // want this as input either on internal or external sync sep
   // but if software sync sep this channel is available I think
         ccmr1.cc1s = 0b01; // ch1 vsync input trigger
         ccmr1.ic1f = 0b00; // no filter on input trigger

         ccmr1.cc2s = 0b00; // ch2 output for irq to start telem rows
         ccmr1.oc2m = 0b000; // Frozen only want irq on match
         ccmr1.oc2pe = false; // want to be able to update on the fly
         line_counter::get()->ccmr1.set (ccmr1.value);
      }

      {
         quan::stm32::tim::ccer_t ccer = line_counter::get()->ccer.get();
   #if !defined QUAN_OSD_SOFTWARE_SYNCSEP
   // external vsync pin
         ccer.cc1np = false; // input trigger falling edge
         ccer.cc1p  = true; // input trigger falling edge
   #else
      #if !defined QUAN_AERFLITE_BOARD
     /*
         with software sync sep then
         no external trigger used  for enabling line_counter
         rather is done by software sync sep routines
     */
     #else
     // aerflite TIM3_CH1 csync input positive pulse
         ccer.cc1np = false; // input trigger rising edge
         ccer.cc1p  = false; // input trigger rising edge
     #endif
   #endif
         ccer.cc2e = true;  // enable cc2 output for telem begin
         line_counter::get()->ccer.set (ccer.value);
      }
      {
         quan::stm32::tim::ccmr2_t ccmr2 = line_counter::get()->ccmr2.get();
         ccmr2.cc3s = 0b00; // ch3 output for telem end
         ccmr2.oc3m = 0b000; // Frozen only want irq on match
         ccmr2.oc3pe = false; // want to be able to update on the fly
         ccmr2.cc4s = 0b00; // ch4 output for first visible osd row
         ccmr2.oc4m = 0b000; // Frozen only want irq on match
         ccmr2.oc4pe = false; // want to be able to update on the fly
         line_counter::get()->ccmr2.set (ccmr2.value);
      }
      typedef video_cfg::rows::telem telem;
      line_counter::get()->ccr2 = telem::m_begin -1 ;
      line_counter::get()->ccr3 = telem::m_end - 1;
      // interlace means jump 2 rows per clk
      typedef video_cfg::rows::osd osd;
      line_counter::get()->ccr4 = osd::m_begin/2-1 ;
      line_counter::get()->arr = osd::get_end()/2 - 2;
      {
         quan::stm32::tim::dier_t dier = line_counter::get()->dier.get();
         dier.cc2ie = true;
         dier.cc3ie = true;
         dier.cc4ie = true;
         dier.uie = true;
         line_counter::get()->dier.set (dier.value);
      }
      
      line_counter::get()->cnt = 0 ;
      NVIC_SetPriority(TIM3_IRQn,interrupt_priority::video);
      NVIC_EnableIRQ (TIM3_IRQn);

   }
}// namespace

namespace detail{
   void internal_video_mode_setup();
   void do_internal_video_mode_irq();
}

void video_cfg::rows::setup()
{
   if (osd_state::get() == osd_state::external_video){
     video_cfg_rows_external_setup();
   }else{
     detail::internal_video_mode_setup();
   }
}

// line counter irq
extern "C" void TIM3_IRQHandler() __attribute__ ( (interrupt ("IRQ")));

extern "C" void TIM3_IRQHandler()
{
   if (osd_state::get() == osd_state::external_video){
      typedef video_cfg::rows rows;  
      uint16_t const sr = rows::line_counter::get()->sr.get();
      if ( sr & (1 << 2)) { // cc2_if
         rows::line_counter::get()->sr.bb_clearbit<2>();
         rows::telem::begin();
      }else {
         if( sr & (1 << 3)){ // cc3_if
            rows::line_counter::get()->sr.bb_clearbit<3>();
            rows::telem::end();
         }else{
            if( sr & (1 << 4)){ //cc4_if
               rows::line_counter::get()->sr.bb_clearbit<4>();
               rows::osd::begin();
            }else{
               rows::line_counter::get()->sr.bb_clearbit<0>();  // must be uif
               rows::osd::end();
               return;
            }
         }
      }
   }else{
      detail::do_internal_video_mode_irq();
   }
}


