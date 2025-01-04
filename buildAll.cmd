@echo off
FOR /L %%X IN (2017,1,2026) DO (
	if NOT DEFINED ADSK_3DSMAX_SDK_%%X CONTINUE
	MSBuild PS_Schematic.sln /P:Configuration=Release-Max%%X
	MSBuild PS_Schematic.sln /P:Configuration=Release-Max%%X /P:KRAKATOA_BUILD=Krakatoa
)