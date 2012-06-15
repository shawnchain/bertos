#
# User makefile.
# Edit this file to change compiler options and related stuff.
#

# Programmer interface configuration, see http://dev.bertos.org/wiki/ProgrammerInterface for help
arduino_kiss_tnc_PROGRAMMER_TYPE = none
arduino_kiss_tnc_PROGRAMMER_PORT = none

# Files included by the user.
arduino_kiss_tnc_USER_CSRC = \
	$(arduino_kiss_tnc_SRC_PATH)/main.c \
	$(arduino_kiss_tnc_HW_PATH)/hw/hw_afsk.c \
	#

# Files included by the user.
arduino_kiss_tnc_USER_PCSRC = \
	#

# Files included by the user.
arduino_kiss_tnc_USER_CPPASRC = \
	#

# Files included by the user.
arduino_kiss_tnc_USER_CXXSRC = \
	#

# Files included by the user.
arduino_kiss_tnc_USER_ASRC = \
	#

# Flags included by the user.
arduino_kiss_tnc_USER_LDFLAGS = \
	#

# Flags included by the user.
arduino_kiss_tnc_USER_CPPAFLAGS = \
	#

# Flags included by the user.
arduino_kiss_tnc_USER_CPPFLAGS = \
	-fno-strict-aliasing \
	-fwrapv \
	#
