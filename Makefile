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
	$(CP) CloudTools.Common/$(OBJDIR)/*.a $(OBJDIR)

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
	$(MAKE) -C AHN.Buildings.Parallel
	$(MAKE) -C AHN.Buildings.Aggregate
	$(MAKE) -C AHN.Buildings.Verify
	$(CP) AHN.Buildings/$(OBJDIR)/*.a $(OBJDIR)
	$(CP) AHN.Buildings/$(OBJDIR)/ahn_buildings $(OBJDIR)
	$(CP) AHN.Buildings.Parallel/$(OBJDIR)/ahn_buildings_par $(OBJDIR)
	$(CP) AHN.Buildings.Aggregate/$(OBJDIR)/ahn_buildings_agg $(OBJDIR)
	$(CP) AHN.Buildings.Verify/$(OBJDIR)/ahn_buildings_ver $(OBJDIR)

.PHONY: clean
clean:
	$(MAKE) -C CloudLib.DEM clean
	$(MAKE) -C CloudTools.Common clean
	$(MAKE) -C CloudTools.DEM.Compare clean
	$(MAKE) -C CloudTools.DEM.Filter clean
	$(MAKE) -C AHN.Buildings clean
	$(MAKE) -C AHN.Buildings.Parallel clean
	$(MAKE) -C AHN.Buildings.Aggregate clean
	$(MAKE) -C AHN.Buildings.Verify clean
	$(RM) $(OBJDIR)/*
