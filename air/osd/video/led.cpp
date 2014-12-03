
#if (QUAN_OSD_BOARD_TYPE != 4)
#include <quan/meta/type_sequence.hpp>
#include <quan/meta/for_each.hpp>
#endif

#include "../resources.hpp"

namespace {
      
   struct do_led_pin_setup
   {
      template <typename Pin>
      void operator()()const
      {
         quan::stm32::module_enable< typename Pin::port_type>();

         quan::stm32::apply<
            Pin
            , quan::stm32::gpio::mode::output
            , quan::stm32::gpio::otype::push_pull
            , quan::stm32::gpio::pupd::none
            , quan::stm32::gpio::ospeed::slow
            , quan::stm32::gpio::ostate::low
         >();
      }
   };
}
#if (QUAN_OSD_BOARD_TYPE != 4)
namespace  quan{ namespace impl{
   template<> struct is_model_of_impl<
      quan::meta::PolymorphicFunctor<1,0>,do_led_pin_setup 
   > : quan::meta::true_{};
}}
#endif

void setup_leds()
{
#if (QUAN_OSD_BOARD_TYPE == 4)
      do_led_pin_setup{}.operator()<heartbeat_led_pin>();
#else
   typedef quan::meta::type_sequence<
    //  red_led_pin , // used for heartbeat led
      blue_led_pin     
      ,green_led_pin  
      ,orange_led_pin 
   > led_pins;
   quan::meta::for_each<led_pins,do_led_pin_setup>{}();
 #endif
}