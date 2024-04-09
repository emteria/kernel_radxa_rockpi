# Makefile for the Himax touchscreen drivers.

    himax_mmi-objs += himax_debug.o
    himax_mmi-objs += himax_inspection.o
    himax_mmi-objs += himax_ic_incell_core.o
    himax_mmi-objs += himax_common.o
    himax_mmi-objs += himax_platform.o
    himax_mmi-objs += Himax_firmware.o
    obj-y += himax_ic_HX83102.o
    obj-y += himax_mmi.o



# 	ccflags-y += -D__EMBEDDED_FW__
    ld_array_start_str = _binary_$(srctree)/$(src)_Himax_firmware_bin_start
    ld_array_start_sym = $(subst -,_,$(subst .,_,$(subst /,_,$(ld_array_start_str))))
    obj_array_start_sym = _binary___Himax_firmware_bin_start
    
    ld_array_size_str = _binary_$(srctree)/$(src)_Himax_firmware_bin_size
    ld_array_size_sym = $(subst -,_,$(subst .,_,$(subst /,_,$(ld_array_size_str))))
    obj_array_size_sym = _binary___Himax_firmware_bin_size
    
    ld_array_end_str = _binary_$(srctree)/$(src)_Himax_firmware_bin_end
    ld_array_end_sym = $(subst -,_,$(subst .,_,$(subst /,_,$(ld_array_end_str))))
    obj_array_end_sym = _binary___Himax_firmware_bin_end
    
    $(src)/Himax_firmware.o: $(src)/Himax_firmware.bin FORCE
		$(LD) $(LDFLAGS) -r -b binary $(srctree)/$(src)/Himax_firmware.bin -o $(objtree)/$(obj)/Himax_firmware.o
		$(OBJCOPY) --redefine-sym $(ld_array_start_sym)=$(obj_array_start_sym) $(objtree)/$(obj)/Himax_firmware.o
		$(OBJCOPY) --redefine-sym $(ld_array_size_sym)=$(obj_array_size_sym) $(objtree)/$(obj)/Himax_firmware.o
		$(OBJCOPY) --redefine-sym $(ld_array_end_sym)=$(obj_array_end_sym) $(objtree)/$(obj)/Himax_firmware.o
