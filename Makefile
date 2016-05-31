CXX     = g++
RM      = rm -f
MKDIR   = mkdir -p
CP      = cp -f

OBJDIR  = Release/Linux

all: CloudLib CloudTools

.PHONY: dirs
dirs:
	$(MKDIR) $(OBJDIR)

CloudLib: CloudLib.DEM

CloudLib.DEM: dirs
	$(MAKE) -C CloudLib.DEM
	$(CP) CloudLib.DEM/$(OBJDIR)/*.a $(OBJDIR)

CloudTools: CloudTools.DEM

CloudTools.DEM: dirs
	$(MAKE) -C CloudTools.Common
	$(MAKE) -C CloudTools.DEM.Compare
	$(MAKE) -C CloudTools.DEM.Filter
	$(CP) CloudTools.DEM.Compare/$(OBJDIR)/dem_compare $(OBJDIR)
	$(CP) CloudTools.DEM.Filter/$(OBJDIR)/dem_filter $(OBJDIR)

clean:
	$(MAKE) -C CloudLib.DEM clean
	$(MAKE) -C CloudTools.Common clean
	$(MAKE) -C CloudTools.DEM.Compare clean
	$(MAKE) -C CloudTools.DEM.Filter clean
	$(RM) $(OBJDIR)/*
