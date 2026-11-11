#ifndef INPUT_METHOD_MANAGER_4DPLUGIN_H
#define INPUT_METHOD_MANAGER_4DPLUGIN_H

#include "4DPluginAPI.h"

// --- Imm Context
static void IM_Associate(PA_PluginParameters params);
static void IM_Disassociate(PA_PluginParameters params);

// --- Imm Status
static void IM_Get_mode(PA_PluginParameters params);
static void IM_Set_mode(PA_PluginParameters params);
static void IM_Get_open_status(PA_PluginParameters params);
static void IM_Set_open_status(PA_PluginParameters params);
static void IM_Get_input_mode(PA_PluginParameters params);
static void IM_Set_input_mode(PA_PluginParameters params);

// --- Imm Composition
static void IM_Get_composition(PA_PluginParameters params);
static void IM_Complete_composition(PA_PluginParameters params);
static void IM_Cancel_composition(PA_PluginParameters params);

// --- Imm Input Method
static void IM_Get_input_methods(PA_PluginParameters params);
static void IM_Get_input_method(PA_PluginParameters params);
static void IM_Set_input_method(PA_PluginParameters params);

// --- Imm Reading
static void IM_Set_reading_capture(PA_PluginParameters params);
static void IM_Get_reading(PA_PluginParameters params);
static void IM_Get_phonetic(PA_PluginParameters params);
static void IM_Convert_kana(PA_PluginParameters params);

#endif
