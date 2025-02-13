##########################################################################################################################
# File automatically-generated by tool: [projectgenerator] version: [3.6.0] date: [Fri Jan 24 14:15:57 GMT 2020] 
##########################################################################################################################

# ------------------------------------------------
# Generic Makefile (based on gcc)
#
# ChangeLog :
#	2017-02-10 - Several enhancements + project update mode
#   2015-07-22 - first version
# ------------------------------------------------

######################################
# target
######################################
TARGET = prepas_test_example


######################################
# building variables
######################################
# debug build?
DEBUG = 1
# optimization
OPT = -Og -std=c99 #-DPREPAS_EMBEDDED


#######################################
# paths
#######################################
# Build path
BUILD_DIR = build

# doc/doxygen path
DOC_DIR = doc
DOXY_WARN_LOGFILE = doxy_warn_log_file.txt

######################################
# source
######################################
# C sources
C_SOURCES =  \
main.c \
previpass.c \
previpass_util.c

#toto.c

# ASM sources
ASM_SOURCES =


#######################################
# binaries
#######################################
#PREFIX = arm-none-eabi-
PREFIX =
# The gcc compiler bin path can be either defined in make command via GCC_PATH variable (> make GCC_PATH=xxx)
# either it can be added to the PATH environment variable.
ifdef GCC_PATH
CC = $(GCC_PATH)/$(PREFIX)gcc
AS = $(GCC_PATH)/$(PREFIX)gcc -x assembler-with-cpp
CP = $(GCC_PATH)/$(PREFIX)objcopy
SZ = $(GCC_PATH)/$(PREFIX)size
else
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size
endif
HEX = $(CP) -O ihex
BIN = $(CP) -O binary -S
 
#######################################
# CFLAGS
#######################################
# cpu
CPU = -mcpu=cortex-m4

# fpu
FPU = -mfpu=fpv4-sp-d16

# float-abi
FLOAT-ABI = -mfloat-abi=hard

# mcu
#MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)
MCU = 

# macros for gcc
# AS defines
AS_DEFS = 

# C defines
C_DEFS = 

# AS includes
AS_INCLUDES = 

# C includes
C_INCLUDES =  \
-I.

# compile gcc flags
#ASFLAGS = $(MCU) $(AS_DEFS) $(AS_INCLUDES) $(OPT) -Wall -Werror -fdata-sections -ffunction-sections
ASFLAGS = $(MCU) $(AS_DEFS) $(AS_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

#CFLAGS = $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -Wall -Werror -fdata-sections -ffunction-sections
CFLAGS = $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

ifeq ($(DEBUG), 1)
CFLAGS += -g -gdwarf-2
endif


# Generate dependency information
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"


#######################################
# LDFLAGS
#######################################
# link script
LDSCRIPT =

# libraries
#LIBS = -lc -lm -lnosys 
LIBS = -lc -lm
LIBDIR = 
#LDFLAGS = $(MCU) -specs=nano.specs -T$(LDSCRIPT) $(LIBDIR) $(LIBS) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections
LDFLAGS = $(MCU) $(LIBDIR) $(LIBS) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections

# default action: build all
all: $(BUILD_DIR)/$(TARGET).elf

# generate doc (doxygen framework), configure some dynamic parameters from Makefile
doc: Doxyfile
	(cat $< ; \
echo $(CFLAGS) | xargs -n1 echo | grep "^-D" | sed 's/-D//' | xargs echo PREDEFINED= ; \
echo OUTPUT_DIRECTORY=$(DOC_DIR) ; \
echo EXCLUDE=$(DOC_DIR) ; \
echo INCLUDE_PATH=$(subst -I,,$(C_INCLUDES)) ; \
echo WARN_LOGFILE=$(DOXY_WARN_LOGFILE)) \
| doxygen -
	(git log -n 1 > $(DOC_DIR)/doc_version.txt)

#######################################
# build the application
#######################################
# list of objects
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
# list of ASM program objects
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR) 
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@
	
$(BUILD_DIR):
	mkdir $@		

#######################################
# clean up
#######################################
clean:
	-rm -fR $(BUILD_DIR)

doc_clean:
	-rm -fR $(DOC_DIR)
	-rm -f  $(DOXY_WARN_LOGFILE)

#######################################
# dependencies
#######################################
-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: doc doc_clean

# *** EOF ***
