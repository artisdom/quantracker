
ifeq ($(QUANTRACKER_ROOT_DIR), )
$(error "QUANTRACKER_ROOT_DIR must be defined to the path to the quantracker root directory.")
endif

HAVE_DEPENDENCIES_FILE := $(shell if test -f $(QUANTRACKER_ROOT_DIR)Dependencies.mk; then echo "True"; fi)

ifeq ($(HAVE_DEPENDENCIES_FILE), )
  quantracker-make-help:
	@echo ' '
	@echo '   ########## HELP - OSD firmware build needs more info ############'
	@echo '   #                                                               #'
	@echo '   #            Hi. Welcome to quantracker / air / OSD.            #'
	@echo '   #                                                               #'
	@echo '   #            To build the OSD firmware, you need to             #'
	@echo '   #            create a Dependencies.mk file.                     #'
	@echo '   #                                                               #'
	@echo '   #            Please read "Sample-Dependencies.mk" .             #'
	@echo '   #            in this directory for further Details.             #                                                          #'
	@echo '   #                                                               #'
	@echo '   #################################################################'
	@echo ' '	
else

# You will need a custom Dependencies.mk
include $(QUANTRACKER_ROOT_DIR)Dependencies.mk

###############################################################
ifeq ($(TOOLCHAIN_PREFIX), )
$(error "TOOLCHAIN_PREFIX must be defined to the path to the gcc-arm compiler - see README.")
endif

ifeq ($(QUAN_INCLUDE_PATH), )
$(error "QUAN_INCLUDE_PATH must be defined to the path to the quan library - see README.")
endif


ifeq ($(STM32_STD_PERIPH_LIB_DIR), )
$(error "STM32_STD_PERIPH_LIB_DIR must be defined to the path to the STM32 Std peripherals library - see README.")
endif

# board config
#ifeq ($(TELEMETRY_DIRECTION), )
#$(error "TELEMETRY_DIRECTION must be one of QUAN_OSD_TELEM_RECEIVER QUAN_OSD_TELEM_TRANSMITTER QUAN_OSD_TELEM_NONE  - see README.")
#endif

# -------------------board --------------------------------

ifeq ($(AERFLITE),True)
$(info ################### For AERFLITE BOARD ####################)
TARGET_LIB_NAME_PREFIX = aerflite
DEFINES += QUAN_AERFLITE_BOARD
else
$(info ################### For QUANTRACKER_OSD BOARD ####################)
TARGET_LIB_NAME_PREFIX = quantracker_air
endif

DEFINES += QUAN_OSD_SOFTWARE_SYNCSEP HSE_VALUE=8000000 QUAN_OSD_BOARD_TYPE=4

#-------------------board config -------------------------

DEFINES +=  $(TELEMETRY_DIRECTION) 

#-------------------------- processor -----------------

TARGET_PROCESSOR = STM32F4RGT6

DEFINES += QUAN_STM32F4 STM32F405xx STM32F40_41xxx

PROCESSOR_FLAGS += -march=armv7e-m -mtune=cortex-m4 -mhard-float -mthumb \
-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mthumb -mfloat-abi=hard

STM32_INCLUDES = $(STM32_STD_PERIPH_LIB_DIR)CMSIS/Include \
$(STM32_STD_PERIPH_LIB_DIR)CMSIS/Device/ST/STM32F4xx/Include \
$(STM32_STD_PERIPH_LIB_DIR)STM32F4xx_StdPeriph_Driver/inc

# -------rtos -----------------------------------------

ifeq ($(FREE_RTOS_DIR), )
$(error "FREE_RTOS_DIR must be defined to the path to the FreeRTOS library - see README.")
endif

DEFINES += QUAN_FREERTOS 

RTOS_INCLUDES = \
$(FREE_RTOS_DIR)Source/include/ \
$(FREE_RTOS_DIR)Source/portable/GCC/ARM_CM4F \
$(shell pwd)

#----------------includes ------------------------------

INCLUDES = $(STM32_INCLUDES) $(QUAN_INCLUDE_PATH) $(QUANTRACKER_ROOT_DIR)include \
$(RTOS_INCLUDES)

DEFINE_ARGS = $(patsubst %,-D%,$(DEFINES))
INCLUDE_ARGS = $(patsubst %,-I%,$(INCLUDES))

#---------------C++ compiler ------------------------

CC      = $(TOOLCHAIN_PREFIX)bin/arm-none-eabi-g++
CC1     = $(TOOLCHAIN_PREFIX)bin/arm-none-eabi-gcc

ifeq ($(OPTIMISATION_LEVEL), )
OPTIMISATION_LEVEL = O3
endif

ifeq ( $(CFLAG_EXTRAS), )
CFLAG_EXTRAS = -fno-math-errno
endif

#required for Ubuntu 12.x placid as system headers have been put in strange places
# these have beeen defined to thos in my bash .profile
CPLUS_INCLUDE_PATH=
C_INCLUDE_PATH=
LIBRARY_PATH=

CFLAGS  = -Wall -Wdouble-promotion -std=gnu++11 -fno-rtti -fno-exceptions -c -g \
-$(OPTIMISATION_LEVEL) $(DEFINE_ARGS) $(INCLUDE_ARGS) $(PROCESSOR_FLAGS) \
 $(CFLAG_EXTRAS) -fno-math-errno -Wl,-u,vsprintf -lm -fdata-sections -ffunction-sections\
-Wno-unused-local-typedefs

#--------linking ----------------------------

#INIT_LIB_PREFIX = $(TOOLCHAIN_PREFIX)/lib/gcc/arm-none-eabi/$(TOOLCHAIN_GCC_VERSION)/armv7e-m/fpu/
#INIT_LIBS = $(INIT_LIB_PREFIX)crti.o $(INIT_LIB_PREFIX)crtn.o 

STATIC_LIBRARY_PATH = $(QUANTRACKER_ROOT_DIR)lib/osd/
LD      = $(TOOLCHAIN_PREFIX)bin/arm-none-eabi-g++
CP      = $(TOOLCHAIN_PREFIX)bin/arm-none-eabi-objcopy
OD      = $(TOOLCHAIN_PREFIX)bin/arm-none-eabi-objdump
SIZ     = $(TOOLCHAIN_PREFIX)bin/arm-none-eabi-size
LDFLAGS = -T$(LINKER_SCRIPT) -$(OPTIMISATION_LEVEL)  -nodefaultlibs \
 $(PROCESSOR_FLAGS) --specs=nano.specs $(CFLAG_EXTRAS) -Wl,--gc-sections

CPFLAGS = -Obinary
ODFLAGS = -d 

# The actual libraries are defined in the 
static_libraries = $(patsubst %,$(STATIC_LIBRARY_PATH)%,$(static_library_files))
#If have dependencies file
endif
