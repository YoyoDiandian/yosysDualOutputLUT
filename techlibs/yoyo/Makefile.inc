OBJS += techlibs/yoyo/lut6d_map.o

$(eval $(call add_share_file,share/yoyo,techlibs/yoyo/cells_sim.v))
$(eval $(call add_share_file,share/yoyo,techlibs/yoyo/cells_lib.v))