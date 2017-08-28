#
# User makefile.
# Edit this file to change compiler options and related stuff.
#

# Programmer interface configuration, see http://dev.bertos.org/wiki/ProgrammerInterface for help
WxRx_PROGRAMMER_TYPE = none
WxRx_PROGRAMMER_PORT = none

# Files included by the user.
WxRx_USER_CSRC = \
	$(WxRx_SRC_PATH)/main.c \
	$(WxRx_SRC_PATH)/rtc.c \
	$(WxRx_SRC_PATH)/eeprommap.c \
	#

# Files included by the user.
WxRx_USER_PCSRC = \
	#

# Files included by the user.
WxRx_USER_CPPASRC = \
	#

# Files included by the user.
WxRx_USER_CXXSRC = \
	#

# Files included by the user.
WxRx_USER_ASRC = \
	#

# Flags included by the user.
WxRx_USER_LDFLAGS = \
	#

# Flags included by the user.
WxRx_USER_CPPAFLAGS = \
	#

# Flags included by the user.
WxRx_USER_CPPFLAGS = \
	-fno-strict-aliasing \
	-fwrapv \
	#
