include Makefile.config

.PHONY: all
all: $(OBJDIR) CloudLib CloudTools AHN

$(OBJDIR):
	$(MKDIR) $(OBJDIR)

.PHONY: CloudLib
CloudLib: CloudLib.DEM

.PHONY: CloudLib.DEM
CloudLib.DEM: $(OBJDIR)
	$(MAKE) -C CloudLib.DEM
	$(CP) CloudLib.DEM/$(OBJDIR)/*.a $(OBJDIR)

.PHONY: CloudTools
CloudTools: CloudTools.Common CloudTools.DEM

.PHONY: CloudTools.Common
CloudTools.Common: $(OBJDIR)
	$(MAKE) -C CloudTools.Common

.PHONY: CloudTools.DEM
CloudTools.DEM: $(OBJDIR) CloudLib.DEM CloudTools.Common
	$(MAKE) -C CloudTools.DEM.Compare
	$(MAKE) -C CloudTools.DEM.Filter
	$(CP) CloudTools.DEM.Compare/$(OBJDIR)/dem_compare $(OBJDIR)
	$(CP) CloudTools.DEM.Filter/$(OBJDIR)/dem_filter $(OBJDIR)

.PHONY: AHN
AHN: AHN.Buildings

.PHONY: AHN.Buildings
AHN.Buildings: $(OBJDIR) CloudLib.DEM CloudTools.Common
	$(MAKE) -C AHN.Buildings
	$(CP) AHN.Buildings/$(OBJDIR)/ahn_buildings $(OBJDIR)

.PHONY: clean
clean:
	$(MAKE) -C CloudLib.DEM clean
	$(MAKE) -C CloudTools.Common clean
	$(MAKE) -C CloudTools.DEM.Compare clean
	$(MAKE) -C CloudTools.DEM.Filter clean
	$(MAKE) -C AHN.Buildings clean
	$(RM) $(OBJDIR)/*
