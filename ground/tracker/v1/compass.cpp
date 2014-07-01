

#include <stm32f4xx.h>
#include <quan/stm32/rcc.hpp>
#include <quan/stm32/f4/exti/set_exti.hpp>
#include <quan/stm32/f4/syscfg/module_enable_disable.hpp>

#include "resources.hpp"
#include "events.hpp"
#include <quan/three_d/vect.hpp>
#include <quan/constrain.hpp>
#include "compass.hpp"

quan::three_d::vect<float> raw_compass ::value = {0.f,0.f,0.f};
int32_t raw_compass::strap_value = 0;
float raw_compass::filter_value = 0.1f;
bool raw_compass::m_request_disable_updating = false;
bool raw_compass::m_updating_enabled = true;

void raw_compass::set_strap(int32_t val)
{
   switch(val){
      case 0:
      case 1:
      case -1:
      strap_value = val;
      break;
      default:
      break;
   }
}

int32_t raw_compass::get_strap()
{
   return strap_value;
}

void raw_compass::set_filter(float const & val)
{
   filter_value = quan::constrain(val,0.f,1.f);
}

void raw_compass::clear()
{
   value = {0.f,0.f,0.f};
}

quan::three_d::vect<float> const& raw_compass::get()
{
   return value;
}

int32_t  ll_update_mag(quan::three_d::vect<int16_t> & result_out,int32_t strap);

void raw_compass::request_disable_updating()
{
   if ( m_updating_enabled){
      m_request_disable_updating = true;
   }
}

void raw_compass::enable_updating()
{
   m_request_disable_updating = false;
   m_updating_enabled = true;
}

int32_t raw_compass::update()
{
   if (m_updating_enabled){
      quan::three_d::vect<int16_t> result;
      int res = ::ll_update_mag(result,raw_compass::get_strap());
      if ( res == 1){
         raw_compass::value 
            = raw_compass::value * (1.f - raw_compass::filter_value) 
               + result * raw_compass::filter_value;
         // Disable updating here because its the end of a cycle so nice and neat
         if ( m_request_disable_updating){
            m_request_disable_updating = false;
            m_updating_enabled = false;
         }
      }
      return res;
   }else{
      return false;
   }
}

namespace {

   void setup_mag_ready_irq()
   {
      quan::stm32::module_enable<quan::stm32::syscfg>(); 
      quan::stm32::set_exti_syscfg<mag_rdy_exti_pin>();
      quan::stm32::set_exti_falling_edge<mag_rdy_exti_pin>();
      NVIC_SetPriority(I2C1_EV_IRQn,interrupt_priority::exti_mag_rdy);
      quan::stm32::nvic_enable_exti_irq<mag_rdy_exti_pin>();
      quan::stm32::module_enable<mag_rdy_exti_pin::port_type>();
      quan::stm32::apply<
         mag_rdy_exti_pin
         , quan::stm32::gpio::mode::input
         , quan::stm32::gpio::pupd::none // make this pullup ok as mag is on 3v?
      >();
      
      quan::stm32::enable_exti_interrupt<mag_rdy_exti_pin>();
   }
}

void raw_compass::init()
{
   setup_mag_ready_irq();
   NVIC_SetPriority(I2C1_EV_IRQn,interrupt_priority::i2c_mag_evt);
   // use at 100kHz I2C. At 400 kHz on 500 mm long lines get errors
   i2c_mag_port::init(false,false);
}

extern "C" void I2C1_EV_IRQHandler() __attribute__ ((interrupt ("IRQ")));
extern "C" void I2C1_EV_IRQHandler()
{     
   static_assert(std::is_same<i2c_mag_port::i2c_type, quan::stm32::i2c1>::value,"incorrect port irq");
   i2c_mag_port::handle_irq();
}

extern "C" void I2C1_ER_IRQHandler() __attribute__ ((interrupt ("IRQ")));
extern "C" void I2C1_ER_IRQHandler()
{
   static_assert(std::is_same<i2c_mag_port::i2c_type, quan::stm32::i2c1>::value,"incorrect port irq");
   uint32_t const sr1 = i2c_mag_port::i2c_type::get()->sr1.get();
   i2c_mag_port::i2c_type::get()->sr1.set(sr1 & 0xFF); 
   i2c_mag_port::i2c_errno = i2c_mag_port::errno_t::i2c_err_handler;
}

extern "C" void EXTI1_IRQHandler() __attribute__ ((interrupt ("IRQ")));
extern "C" void EXTI1_IRQHandler()
{      
   if (quan::stm32::is_event_pending<mag_rdy_exti_pin>()){
      mag_rdy_event.set();
      quan::stm32::clear_event_pending<mag_rdy_exti_pin>();
   }else{
      i2c_mag_port::i2c_errno = i2c_mag_port::errno_t::unknown_exti_irq;
   }
}