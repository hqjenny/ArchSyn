
LEVEL = ../../..
LIBRARYNAME = LLVMDecoupleInsScc
LOADABLE_MODULE = 1
USEDLIBS =

# If we don't need RTTI or EH, there's no reason to export anything
# from the hello plugin.
ifneq ($(REQUIRES_RTTI), 1)
ifneq ($(REQUIRES_EH), 1)
EXPORTED_SYMBOL_FILE = $(PROJ_SRC_DIR)/DecoupleInsScc.exports
endif
endif

include $(LEVEL)/Makefile.common

