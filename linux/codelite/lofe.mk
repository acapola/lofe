##
## Auto Generated makefile by CodeLite IDE
## any manual changes will be erased      
##
## Debug
ProjectName            :=lofe
ConfigurationName      :=Debug
WorkspacePath          := "/home/seb/lofe/linux/codelite"
ProjectPath            := "/home/seb/lofe/linux/codelite"
IntermediateDirectory  :=./Debug
OutDir                 := $(IntermediateDirectory)
CurrentFileName        :=
CurrentFilePath        :=
CurrentFileFullPath    :=
User                   :=sebastien riou
Date                   :=09/03/15
CodeLitePath           :="/home/seb/.codelite"
LinkerName             :=/usr/bin/g++
SharedObjectLinkerName :=/usr/bin/g++ -shared -fPIC
ObjectSuffix           :=.o
DependSuffix           :=.o.d
PreprocessSuffix       :=.i
DebugSwitch            :=-g 
IncludeSwitch          :=-I
LibrarySwitch          :=-l
OutputSwitch           :=-o 
LibraryPathSwitch      :=-L
PreprocessorSwitch     :=-D
SourceSwitch           :=-c 
OutputFile             :=$(IntermediateDirectory)/$(ProjectName)
Preprocessors          :=$(PreprocessorSwitch)_FILE_OFFSET_BITS=64 
ObjectSwitch           :=-o 
ArchiveOutputSwitch    := 
PreprocessOnlySwitch   :=-E
ObjectsFileList        :="lofe.txt"
PCHCompileFlags        :=
MakeDirCommand         :=mkdir -p
LinkOptions            :=  
IncludePath            :=  $(IncludeSwitch). $(IncludeSwitch). $(IncludeSwitch)../../x86 $(IncludeSwitch)../../generic 
IncludePCH             := 
RcIncludePath          := 
Libs                   := $(LibrarySwitch)fuse 
ArLibs                 :=  "fuse" 
LibPath                := $(LibraryPathSwitch). 

##
## Common variables
## AR, CXX, CC, AS, CXXFLAGS and CFLAGS can be overriden using an environment variables
##
AR       := /usr/bin/ar rcu
CXX      := /usr/bin/g++
CC       := /usr/bin/gcc
CXXFLAGS :=  -g -O0 -Wall -msse2 -msse -march=native -maes $(Preprocessors)
CFLAGS   :=  -g -O0 -Wall -msse2 -msse -march=native -maes $(Preprocessors)
ASFLAGS  := 
AS       := /usr/bin/as


##
## User defined environment variables
##
CodeLiteDir:=/usr/share/codelite
Objects0=$(IntermediateDirectory)/x86_aes-ni.c$(ObjectSuffix) $(IntermediateDirectory)/linux_lofe_vfs.c$(ObjectSuffix) $(IntermediateDirectory)/generic_lofe.cpp$(ObjectSuffix) 



Objects=$(Objects0) 

##
## Main Build Targets 
##
.PHONY: all clean PreBuild PrePreBuild PostBuild MakeIntermediateDirs
all: $(OutputFile)

$(OutputFile): $(IntermediateDirectory)/.d $(Objects) 
	@$(MakeDirCommand) $(@D)
	@echo "" > $(IntermediateDirectory)/.d
	@echo $(Objects0)  > $(ObjectsFileList)
	$(LinkerName) $(OutputSwitch)$(OutputFile) @$(ObjectsFileList) $(LibPath) $(Libs) $(LinkOptions)

MakeIntermediateDirs:
	@test -d ./Debug || $(MakeDirCommand) ./Debug


$(IntermediateDirectory)/.d:
	@test -d ./Debug || $(MakeDirCommand) ./Debug

PreBuild:


##
## Objects
##
$(IntermediateDirectory)/x86_aes-ni.c$(ObjectSuffix): ../../x86/aes-ni.c $(IntermediateDirectory)/x86_aes-ni.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/seb/lofe/x86/aes-ni.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/x86_aes-ni.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/x86_aes-ni.c$(DependSuffix): ../../x86/aes-ni.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/x86_aes-ni.c$(ObjectSuffix) -MF$(IntermediateDirectory)/x86_aes-ni.c$(DependSuffix) -MM "../../x86/aes-ni.c"

$(IntermediateDirectory)/x86_aes-ni.c$(PreprocessSuffix): ../../x86/aes-ni.c
	@$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/x86_aes-ni.c$(PreprocessSuffix) "../../x86/aes-ni.c"

$(IntermediateDirectory)/linux_lofe_vfs.c$(ObjectSuffix): ../lofe_vfs.c $(IntermediateDirectory)/linux_lofe_vfs.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/seb/lofe/linux/lofe_vfs.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/linux_lofe_vfs.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/linux_lofe_vfs.c$(DependSuffix): ../lofe_vfs.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/linux_lofe_vfs.c$(ObjectSuffix) -MF$(IntermediateDirectory)/linux_lofe_vfs.c$(DependSuffix) -MM "../lofe_vfs.c"

$(IntermediateDirectory)/linux_lofe_vfs.c$(PreprocessSuffix): ../lofe_vfs.c
	@$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/linux_lofe_vfs.c$(PreprocessSuffix) "../lofe_vfs.c"

$(IntermediateDirectory)/generic_lofe.cpp$(ObjectSuffix): ../../generic/lofe.cpp $(IntermediateDirectory)/generic_lofe.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/seb/lofe/generic/lofe.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/generic_lofe.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/generic_lofe.cpp$(DependSuffix): ../../generic/lofe.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/generic_lofe.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/generic_lofe.cpp$(DependSuffix) -MM "../../generic/lofe.cpp"

$(IntermediateDirectory)/generic_lofe.cpp$(PreprocessSuffix): ../../generic/lofe.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/generic_lofe.cpp$(PreprocessSuffix) "../../generic/lofe.cpp"


-include $(IntermediateDirectory)/*$(DependSuffix)
##
## Clean
##
clean:
	$(RM) -r ./Debug/


