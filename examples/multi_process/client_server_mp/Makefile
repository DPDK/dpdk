subdirs := mp_client mp_server

.PHONY: all static shared clean $(subdirs)
all static shared clean: $(subdirs)

$(subdirs):
	$(MAKE) -C $@ $(MAKECMDGOALS)
