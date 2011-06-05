#
# Make rules
# Author: Igor Ivanov Igor.Ivanov@itseez.com 
#

MODULE_EXT = 
SLIB_EXT = lib
DLIB_EXT = dll

ifeq ($(TARGETTYPE), MODULE)
ifneq ($(MODULE_EXT),)
PTARGET = $(OUT_DIR)/$(TARGETNAME).$(MODULE_EXT)
else
PTARGET = $(OUT_DIR)/$(TARGETNAME)
endif	
INSTDIR = $(INST_DIR)/bin
endif

ifeq ($(TARGETTYPE), LIB)
ifneq ($(SLIB_EXT),)
PTARGET = $(OUT_DIR)/$(TARGETNAME).$(SLIB_EXT)
else
PTARGET = $(OUT_DIR)/$(TARGETNAME)
endif	
INSTDIR = $(INST_DIR)/lib
endif

ifeq ($(TARGETTYPE), DLIB)
ifneq ($(DLIB_EXT),)
PTARGET = $(OUT_DIR)/$(TARGETNAME).$(DLIB_EXT)
else
PTARGET = $(OUT_DIR)/$(TARGETNAME)
endif
INSTDIR = $(INST_DIR)/lib
endif

OBJDIR = $(OUT_DIR)/obj_$(TARGETNAME)

CFLAGS += $(ARCH_CFLAGS) $(SYSINCS) $(ADDINC) $(CFLAGS_EXTRA)

AFLAGS += $(ADDINC)


.PHONY: all dirs clean 


all: info $(PTARGET)
	@echo BUILD : Done $(PTARGET) [$@]


######## Compilation ######################################
$(OBJDIR)/%.o : %.c
	@echo BUILD : Compiling... [$@]
	$(CC) $(CFLAGS) $(ARCH_CFLAGS) -c -o $(addprefix $(OBJDIR)/,$*.o) $< 1>> $(OUT_LOG) 2>>$(WRN_LOG)

$(OBJDIR)/%.o : %.cpp
	@echo BUILD : Compiling... [$@]
	$(CC) $(CFLAGS) $(ARCH_CFLAGS) -c -o $(addprefix $(OBJDIR)/,$*.o) $< 1>> $(OUT_LOG) 2>>$(WRN_LOG)

$(OBJDIR)/%.o : %.asm
	@echo BUILD : Compiling... [$@]
	$(AS) $(AFLAGS) -o $(addprefix $(OBJDIR)/,$*.o) $< 1>> $(OUT_LOG) 2>>$(WRN_LOG)


######## Linkage ##########################################
ifeq ($(TARGETTYPE), MODULE)
$(PTARGET): dirs .depend   $(addprefix $(OBJDIR)/,$(OBJS)) $(SOURCES)
	@echo BUILD : Linking... [$@]
#	$(LN) -o $(PTARGET) -Map $(OBJDIR)/.map $(LFLAGS) $(SYSLIBS) $(EXTRALIB) $(addprefix $(OBJDIR)/,$(OBJS)) 1>> $(OUT_LOG) 2>>$(WRN_LOG)

	$(CC) -o $(PTARGET) $(LFLAGS) $(addprefix $(OBJDIR)/,$(OBJS)) $(SYSLIBS) $(EXTRALIB) 1>> $(OUT_LOG) 2>>$(WRN_LOG)
else
$(PTARGET): dirs .depend $(addprefix $(OBJDIR)/,$(OBJS)) $(SOURCES)
	@echo BUILD : Linking... [$@]
	$(AR) rus $(PTARGET) $(addprefix $(OBJDIR)/,$(OBJS)) 1>> $(OUT_LOG) 2>>$(WRN_LOG)
endif

dirs:
	@echo BUILD : Creating [$@]
	@mkdir -p $(OBJDIR) 1>> $(OUT_LOG) 2>>$(WRN_LOG)


######## Installation #####################################
install: all
	@echo BUILD : Installation [$@]
	@mkdir -p $(INSTDIR) 1>> $(OUT_LOG) 2>>$(WRN_LOG)
	@install $(PTARGET) $(INSTDIR)


######## Uninstallation ###################################
uninstall:
	@echo BUILD : Uninstallation [$@]
	$(DEL) -f $(INSTDIR)/* 1>> $(OUT_LOG) 2>>$(WRN_LOG)
	$(DEL) -rf $(INSTDIR) 1>> $(OUT_LOG) 2>>$(WRN_LOG)


######## Cleanup ##########################################
clean:
	@echo BUILD : Cleaning [$@]
	$(DEL) -f *.o *~ *.lib .depend *.err *.map *.a *.exe 1>> $(OUT_LOG) 2>>$(WRN_LOG)
	$(DEL) -f *.gcov *.gcda *.gcno 1>> $(OUT_LOG) 2>>$(WRN_LOG)
	$(DEL) -f $(OBJDIR)/* 1>> $(OUT_LOG) 2>>$(WRN_LOG)
	$(DEL) -rf $(OBJDIR) 1>> $(OUT_LOG) 2>>$(WRN_LOG)
	$(DEL) -f $(PTARGET) 1>> $(OUT_LOG) 2>>$(WRN_LOG)



######## Generate dependences #############################
.depend: $(addprefix $(OBJDIR)/,$(OBJS:%.o=%.dep))
	@echo BUILD : Creating dependences [$@]
	cat $(OBJDIR)/*.dep > $(OBJDIR)/.depend
	$(DEL) -f $(OBJDIR)/*.dep

$(OBJDIR)/%.dep : %.cpp
	@echo BUILD : Generating dependences file [$@]
	$(CC) $(CFLAGS) -MM -MP $< > $@

$(OBJDIR)/%.dep : %.c
	@echo BUILD : Generating dependences file [$@]
	$(CC) $(CFLAGS) -MM $< > $@

$(OBJDIR)/%.dep : %.asm
	@echo BUILD : Generating dependences file [$@]
	$(AS) $(AFLAGS) -MM $< > $@

ifeq (.depend, $(wildcard .depend))
	include $(OBJDIR)/.depend
endif

info:
	@echo 
	@echo ">`pwd`"
	@echo ".............................................."

