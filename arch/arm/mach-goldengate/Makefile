#
# Makefile for the linux kernel.
#

obj-y					:= core.o clock.o
obj-$(CONFIG_MACH_CORTINA_G2)		+= cortina-g2.o g2-acp.o
obj-$(CONFIG_SMP)			+= platsmp.o headsmp.o
obj-$(CONFIG_HOTPLUG_CPU)		+= hotplug.o
#obj-$(CONFIG_CS75XX_SSP)		+= cs75xx_ssp.o
obj-$(CONFIG_CORTINA_G2_PCIE)		+= pcie.o
obj-$(CONFIG_PWM)			+= pwm.o
obj-$(CONFIG_CORTINA_G2_GLOBAL_TIMER)	+= global_timer.o

# Power Management
obj-$(CONFIG_PM)			+= pm.o
