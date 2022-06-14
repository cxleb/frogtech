@echo off
set includes=/I Source /I "%vk_sdk_path%\include" /I vendor\stb /I vendor\imgui
set source=Source\Runtime\Runtime.cpp Source\Runtime\VulkanGpu.cpp Source\Runtime\VulkanUtils.cpp Source\Runtime\Win32.cpp Source\Editor\Entry.cpp vendor\imgui\imgui.cpp vendor\imgui\imgui_demo.cpp vendor\imgui\imgui_draw.cpp vendor\imgui\imgui_tables.cpp vendor\imgui\imgui_widgets.cpp vendor\imgui\backends\imgui_impl_win32.cpp vendor\imgui\backends\imgui_impl_vulkan.cpp 

set libs="%vk_sdk_path%\Lib\GenericCodeGen.lib" "%vk_sdk_path%/Lib/glslang.lib" "%vk_sdk_path%/Lib/HLSL.lib" "%vk_sdk_path%/Lib/MachineIndependent.lib" "%vk_sdk_path%/Lib/OGLCompiler.lib" "%vk_sdk_path%/Lib/OSDependent.lib" "%vk_sdk_path%/Lib/SPIRV.lib" "%vk_sdk_path%/Lib/SPIRV-Tools.lib" "%vk_sdk_path%/Lib/SPIRV-Tools-opt.lib" "%vk_sdk_path%/Lib/SPVRemapper.lib" "%vk_sdk_path%/Lib/vulkan-1.lib"
rem "%vk_sdk_path%/Lib/glslang-default-resource-limits.lib"
cl /nologo /EHsc /MD /std:c++20 %includes% %source% %libs% user32.lib /link /INCREMENTAL

