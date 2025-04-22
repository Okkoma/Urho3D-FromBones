//
// Copyright (c) 2008-2022 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include <Urho3D/Core/Context.h>
#include <Urho3D/Core/ProcessUtils.h>
#include <Urho3D/Core/StringUtils.h>
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/Graphics/GraphicsImpl.h>
#include <Urho3D/Graphics/Shader.h>
#include <Urho3D/IO/File.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/IO/Log.h>

#ifdef WIN32
#include <windows.h>
#endif

#include <Urho3D/DebugNew.h>

#include <vector>
#include <utility>

#include "spirv_reflect.h"

using namespace Urho3D;


const char* vertexElementSemanticNames[] =
{
    "POSITION",
    "NORMAL",
    "BINORMAL",
    "TANGENT",
    "TEXCOORD",
    "COLOR",
    "BLENDWEIGHT",
    "BLENDINDICES",
    "OBJECTINDEX"
};

const char* parametersGroupNames[] =
{
    "Frame",
    "Camera",
    "Zone",
    "Light",
    "Material",
    "Object",
    "Custom",
    0
};

const char* textureUnitNames[] =
{
    "DIFFMAP",
    "NORMALMAP",
    "SPECMAP",
    "EMISSIVEMAP",
    "ENVMAP",
    "VOLUMEMAP",
    "CUSTOMMAP1",
    "CUSTOMMAP2",
    "LIGHTRAMPMAP",
    "LIGHTSPOTMAP",
    "SHADOWMAP",
    "FACESELECTCUBEMAP",
    "INDIRECTIONCUBEMAP",
    "DEPTHBUFFER",
    "LIGHTBUFFER",
    "ZONECUBEMAP"
};

const char* descriptorTypeNames[] =
{
    "VK_DESCRIPTOR_TYPE_SAMPLER",
    "VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER",
    "VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE",
    "VK_DESCRIPTOR_TYPE_STORAGE_IMAGE",
    "VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER",
    "VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER",
    "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER",
    "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER",
    "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC",
    "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC",
    "VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT"
};

String ToStringDescriptorType(SpvReflectDescriptorType value)
{
    switch (value)
    {
    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER                    :
        return "VK_DESCRIPTOR_TYPE_SAMPLER";
    case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER     :
        return "VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER";
    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE              :
        return "VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE";
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE              :
        return "VK_DESCRIPTOR_TYPE_STORAGE_IMAGE";
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER       :
        return "VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER";
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER       :
        return "VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER";
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER             :
        return "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER";
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER             :
        return "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER";
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC     :
        return "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC";
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC     :
        return "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC";
    case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT           :
        return "VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT";
    case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR :
        return "VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR";
    }
    // unhandled SpvReflectDescriptorType enum value
    return "VK_DESCRIPTOR_TYPE_???";
}

String ToStringSpvBuiltIn(SpvBuiltIn built_in)
{
    switch (built_in)
    {
    case SpvBuiltInPosition                    :
        return "Position";
    case SpvBuiltInPointSize                   :
        return "PointSize";
    case SpvBuiltInClipDistance                :
        return "ClipDistance";
    case SpvBuiltInCullDistance                :
        return "CullDistance";
    case SpvBuiltInVertexId                    :
        return "VertexId";
    case SpvBuiltInInstanceId                  :
        return "InstanceId";
    case SpvBuiltInPrimitiveId                 :
        return "PrimitiveId";
    case SpvBuiltInInvocationId                :
        return "InvocationId";
    case SpvBuiltInLayer                       :
        return "Layer";
    case SpvBuiltInViewportIndex               :
        return "ViewportIndex";
    case SpvBuiltInTessLevelOuter              :
        return "TessLevelOuter";
    case SpvBuiltInTessLevelInner              :
        return "TessLevelInner";
    case SpvBuiltInTessCoord                   :
        return "TessCoord";
    case SpvBuiltInPatchVertices               :
        return "PatchVertices";
    case SpvBuiltInFragCoord                   :
        return "FragCoord";
    case SpvBuiltInPointCoord                  :
        return "PointCoord";
    case SpvBuiltInFrontFacing                 :
        return "FrontFacing";
    case SpvBuiltInSampleId                    :
        return "SampleId";
    case SpvBuiltInSamplePosition              :
        return "SamplePosition";
    case SpvBuiltInSampleMask                  :
        return "SampleMask";
    case SpvBuiltInFragDepth                   :
        return "FragDepth";
    case SpvBuiltInHelperInvocation            :
        return "HelperInvocation";
    case SpvBuiltInNumWorkgroups               :
        return "NumWorkgroups";
    case SpvBuiltInWorkgroupSize               :
        return "WorkgroupSize";
    case SpvBuiltInWorkgroupId                 :
        return "WorkgroupId";
    case SpvBuiltInLocalInvocationId           :
        return "LocalInvocationId";
    case SpvBuiltInGlobalInvocationId          :
        return "GlobalInvocationId";
    case SpvBuiltInLocalInvocationIndex        :
        return "LocalInvocationIndex";
    case SpvBuiltInWorkDim                     :
        return "WorkDim";
    case SpvBuiltInGlobalSize                  :
        return "GlobalSize";
    case SpvBuiltInEnqueuedWorkgroupSize       :
        return "EnqueuedWorkgroupSize";
    case SpvBuiltInGlobalOffset                :
        return "GlobalOffset";
    case SpvBuiltInGlobalLinearId              :
        return "GlobalLinearId";
    case SpvBuiltInSubgroupSize                :
        return "SubgroupSize";
    case SpvBuiltInSubgroupMaxSize             :
        return "SubgroupMaxSize";
    case SpvBuiltInNumSubgroups                :
        return "NumSubgroups";
    case SpvBuiltInNumEnqueuedSubgroups        :
        return "NumEnqueuedSubgroups";
    case SpvBuiltInSubgroupId                  :
        return "SubgroupId";
    case SpvBuiltInSubgroupLocalInvocationId   :
        return "SubgroupLocalInvocationId";
    case SpvBuiltInVertexIndex                 :
        return "VertexIndex";
    case SpvBuiltInInstanceIndex               :
        return "InstanceIndex";
    case SpvBuiltInSubgroupEqMaskKHR           :
        return "SubgroupEqMaskKHR";
    case SpvBuiltInSubgroupGeMaskKHR           :
        return "SubgroupGeMaskKHR";
    case SpvBuiltInSubgroupGtMaskKHR           :
        return "SubgroupGtMaskKHR";
    case SpvBuiltInSubgroupLeMaskKHR           :
        return "SubgroupLeMaskKHR";
    case SpvBuiltInSubgroupLtMaskKHR           :
        return "SubgroupLtMaskKHR";
    case SpvBuiltInBaseVertex                  :
        return "BaseVertex";
    case SpvBuiltInBaseInstance                :
        return "BaseInstance";
    case SpvBuiltInDrawIndex                   :
        return "DrawIndex";
    case SpvBuiltInDeviceIndex                 :
        return "DeviceIndex";
    case SpvBuiltInViewIndex                   :
        return "ViewIndex";
    case SpvBuiltInBaryCoordNoPerspAMD         :
        return "BaryCoordNoPerspAMD";
    case SpvBuiltInBaryCoordNoPerspCentroidAMD :
        return "BaryCoordNoPerspCentroidAMD";
    case SpvBuiltInBaryCoordNoPerspSampleAMD   :
        return "BaryCoordNoPerspSampleAMD";
    case SpvBuiltInBaryCoordSmoothAMD          :
        return "BaryCoordSmoothAMD";
    case SpvBuiltInBaryCoordSmoothCentroidAMD  :
        return "BaryCoordSmoothCentroidAMD";
    case SpvBuiltInBaryCoordSmoothSampleAMD    :
        return "BaryCoordSmoothSampleAMD";
    case SpvBuiltInBaryCoordPullModelAMD       :
        return "BaryCoordPullModelAMD";
    case SpvBuiltInFragStencilRefEXT           :
        return "FragStencilRefEXT";
    case SpvBuiltInViewportMaskNV              :
        return "ViewportMaskNV";
    case SpvBuiltInSecondaryPositionNV         :
        return "SecondaryPositionNV";
    case SpvBuiltInSecondaryViewportMaskNV     :
        return "SecondaryViewportMaskNV";
    case SpvBuiltInPositionPerViewNV           :
        return "PositionPerViewNV";
    case SpvBuiltInViewportMaskPerViewNV       :
        return "ViewportMaskPerViewNV";
    case SpvBuiltInLaunchIdKHR                 :
        return "InLaunchIdKHR";
    case SpvBuiltInLaunchSizeKHR               :
        return "InLaunchSizeKHR";
    case SpvBuiltInWorldRayOriginKHR           :
        return "InWorldRayOriginKHR";
    case SpvBuiltInWorldRayDirectionKHR        :
        return "InWorldRayDirectionKHR";
    case SpvBuiltInObjectRayOriginKHR          :
        return "InObjectRayOriginKHR";
    case SpvBuiltInObjectRayDirectionKHR       :
        return "InObjectRayDirectionKHR";
    case SpvBuiltInRayTminKHR                  :
        return "InRayTminKHR";
    case SpvBuiltInRayTmaxKHR                  :
        return "InRayTmaxKHR";
    case SpvBuiltInInstanceCustomIndexKHR      :
        return "InInstanceCustomIndexKHR";
    case SpvBuiltInObjectToWorldKHR            :
        return "InObjectToWorldKHR";
    case SpvBuiltInWorldToObjectKHR            :
        return "InWorldToObjectKHR";
    case SpvBuiltInHitTNV                      :
        return "InHitTNV";
    case SpvBuiltInHitKindKHR                  :
        return "InHitKindKHR";
    case SpvBuiltInIncomingRayFlagsKHR         :
        return "InIncomingRayFlagsKHR";
    case SpvBuiltInRayGeometryIndexKHR         :
        return "InRayGeometryIndexKHR";

    case SpvBuiltInMax:
    default:
        break;
    }
    // unhandled SpvBuiltIn enum value
    String str = String("??? (") + String((int)built_in) + String(")");
    return str;
}

String ToStringScalarType(const SpvReflectTypeDescription& type)
{
    switch(type.op)
    {
    case SpvOpTypeVoid:
    {
        return "void";
        break;
    }
    case SpvOpTypeBool:
    {
        return "bool";
        break;
    }
    case SpvOpTypeInt:
    {
        if (type.traits.numeric.scalar.signedness)
            return "int";
        else
            return "uint";
    }
    case SpvOpTypeFloat:
    {
        switch (type.traits.numeric.scalar.width)
        {
        case 32:
            return "float";
        case 64:
            return "double";
        default:
            break;
        }
    }
    case SpvOpTypeStruct:
    {
        return "struct";
    }
    default:
    {
        break;
    }
    }
    return String::EMPTY;
}

String ToStringGlslType(const SpvReflectTypeDescription& type)
{
    switch (type.op)
    {
    case SpvOpTypeVector:
    {
        switch (type.traits.numeric.scalar.width)
        {
        case 32:
        {
            switch (type.traits.numeric.vector.component_count)
            {
            case 2:
                return "vec2";
            case 3:
                return "vec3";
            case 4:
                return "vec4";
            }
        }
        break;

        case 64:
        {
            switch (type.traits.numeric.vector.component_count)
            {
            case 2:
                return "dvec2";
            case 3:
                return "dvec3";
            case 4:
                return "dvec4";
            }
        }
        break;
        }
    }
    break;
    default:
        break;
    }
    return ToStringScalarType(type);
}

String ToStringHlslType(const SpvReflectTypeDescription& type)
{
    switch (type.op)
    {
    case SpvOpTypeVector:
    {
        switch (type.traits.numeric.scalar.width)
        {
        case 32:
        {
            switch (type.traits.numeric.vector.component_count)
            {
            case 2:
                return "float2";
            case 3:
                return "float3";
            case 4:
                return "float4";
            }
        }
        break;

        case 64:
        {
            switch (type.traits.numeric.vector.component_count)
            {
            case 2:
                return "double2";
            case 3:
                return "double3";
            case 4:
                return "double4";
            }
        }
        break;
        }
    }
    break;

    default:
        break;
    }

    return ToStringScalarType(type);
}

String ToStringType(SpvSourceLanguage src_lang, const SpvReflectTypeDescription& type)
{
    if (src_lang == SpvSourceLanguageHLSL)
    {
        return ToStringHlslType(type);
    }

    return ToStringGlslType(type);
}

String ToStringFormat(SpvReflectFormat fmt)
{
    switch(fmt)
    {
    case SPV_REFLECT_FORMAT_UNDEFINED           :
        return "VK_FORMAT_UNDEFINED";
    case SPV_REFLECT_FORMAT_R32_UINT            :
        return "VK_FORMAT_R32_UINT";
    case SPV_REFLECT_FORMAT_R32_SINT            :
        return "VK_FORMAT_R32_SINT";
    case SPV_REFLECT_FORMAT_R32_SFLOAT          :
        return "VK_FORMAT_R32_SFLOAT";
    case SPV_REFLECT_FORMAT_R32G32_UINT         :
        return "VK_FORMAT_R32G32_UINT";
    case SPV_REFLECT_FORMAT_R32G32_SINT         :
        return "VK_FORMAT_R32G32_SINT";
    case SPV_REFLECT_FORMAT_R32G32_SFLOAT       :
        return "VK_FORMAT_R32G32_SFLOAT";
    case SPV_REFLECT_FORMAT_R32G32B32_UINT      :
        return "VK_FORMAT_R32G32B32_UINT";
    case SPV_REFLECT_FORMAT_R32G32B32_SINT      :
        return "VK_FORMAT_R32G32B32_SINT";
    case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT    :
        return "VK_FORMAT_R32G32B32_SFLOAT";
    case SPV_REFLECT_FORMAT_R32G32B32A32_UINT   :
        return "VK_FORMAT_R32G32B32A32_UINT";
    case SPV_REFLECT_FORMAT_R32G32B32A32_SINT   :
        return "VK_FORMAT_R32G32B32A32_SINT";
    case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT :
        return "VK_FORMAT_R32G32B32A32_SFLOAT";
    case SPV_REFLECT_FORMAT_R64_UINT            :
        return "VK_FORMAT_R64_UINT";
    case SPV_REFLECT_FORMAT_R64_SINT            :
        return "VK_FORMAT_R64_SINT";
    case SPV_REFLECT_FORMAT_R64_SFLOAT          :
        return "VK_FORMAT_R64_SFLOAT";
    case SPV_REFLECT_FORMAT_R64G64_UINT         :
        return "VK_FORMAT_R64G64_UINT";
    case SPV_REFLECT_FORMAT_R64G64_SINT         :
        return "VK_FORMAT_R64G64_SINT";
    case SPV_REFLECT_FORMAT_R64G64_SFLOAT       :
        return "VK_FORMAT_R64G64_SFLOAT";
    case SPV_REFLECT_FORMAT_R64G64B64_UINT      :
        return "VK_FORMAT_R64G64B64_UINT";
    case SPV_REFLECT_FORMAT_R64G64B64_SINT      :
        return "VK_FORMAT_R64G64B64_SINT";
    case SPV_REFLECT_FORMAT_R64G64B64_SFLOAT    :
        return "VK_FORMAT_R64G64B64_SFLOAT";
    case SPV_REFLECT_FORMAT_R64G64B64A64_UINT   :
        return "VK_FORMAT_R64G64B64A64_UINT";
    case SPV_REFLECT_FORMAT_R64G64B64A64_SINT   :
        return "VK_FORMAT_R64G64B64A64_SINT";
    case SPV_REFLECT_FORMAT_R64G64B64A64_SFLOAT :
        return "VK_FORMAT_R64G64B64A64_SFLOAT";
    }
    // unhandled SpvReflectFormat enum value
    return "VK_FORMAT_???";
}

void GetSpirvInfo(String& str, const SpvReflectShaderModule& obj)
{
    str += "\n";
    str += "entry point     : " + String(obj.entry_point_name) + "\n";
    str += "source lang     : " + String(spvReflectSourceLanguage(obj.source_language)) + "\n";
    str += "source lang ver : " + String(obj.source_language_version) + "\n";
    str += "stage           : ";
    switch (obj.shader_stage)
    {
    case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:
        str += "VS";
        break;
    case SPV_REFLECT_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
        str += "HS";
        break;
    case SPV_REFLECT_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
        str += "DS";
        break;
    case SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT:
        str += "GS";
        break;
    case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:
        str += "PS";
        break;
    case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:
        str += "CS";
        break;
    default:
        break;
    }

    str += "\n";
}

void GetSpirvInfo(String& str, SpvSourceLanguage src_lang, const PODVector<SpvReflectInterfaceVariable*>& inputVariables)
{
    str += "\n";

    for (unsigned i=0; i < inputVariables.Size(); i++)
    {
        if (!inputVariables[i])
            continue;

        const SpvReflectInterfaceVariable& obj = *inputVariables[i];

        str += "   location  : ";
        if (obj.decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN)
        {
            str += ToStringSpvBuiltIn(obj.built_in) + " (built-in)";
        }
        else
        {
            str += obj.location;
        }

        str += "\n";

        if (obj.semantic != nullptr)
        {
            str += "   semantic  : " + String(obj.semantic) + "\n";
        }
        str += "   type      : " + ToStringType(src_lang, *obj.type_description) + "\n";
        str += "   format    : " + ToStringFormat(obj.format) + "\n";
        str += "   qualifier : ";
        if (obj.decoration_flags & SPV_REFLECT_DECORATION_FLAT)
        {
            str += "flat";
        }
        else if (obj.decoration_flags & SPV_REFLECT_DECORATION_NOPERSPECTIVE)
        {
            str += "noperspective";
        }
        str += "\n";

        str += "   name      : " + String(obj.name);
        if ((obj.type_description->type_name != nullptr) && (strlen(obj.type_description->type_name) > 0))
        {
            str += " (" + String(obj.type_description->type_name) + ")";
        }

        str += "\n";
    }
}

void GetSpirvInfo(String& str, const PODVector<SpvReflectDescriptorSet*>& descriptorSets)
{
    str += "\n";

    for (unsigned i=0; i < descriptorSets.Size(); i++)
    {
        if (!descriptorSets[i])
            continue;

        const SpvReflectDescriptorSet& descriptorSet = *descriptorSets[i];
        str += "   set           : " + String(descriptorSet.set) + "\n";
        str += "   binding count : " + String(descriptorSet.binding_count) + "\n";

        // bindings
        for (unsigned j = 0; j < descriptorSet.binding_count; ++j)
        {
            const SpvReflectDescriptorBinding& binding = *descriptorSet.bindings[j];
            str += "       binding : " + String(binding.binding) + "\n";
            str += "       type    : " + ToStringDescriptorType(binding.descriptor_type) + "\n";

            // array
            if (binding.array.dims_count > 0)
            {
                str += "       array   : ";
                for (unsigned k = 0; k < binding.array.dims_count; k++)
                    str += "[" + String(binding.array.dims[k]) + "]";
                str += "\n";
            }

            // counter
            if (binding.uav_counter_binding != nullptr)
            {
                str += "       counter : ";
                str += "(";
                str += "set=" + String(binding.uav_counter_binding->set) + ", ";
                str += "binding=" + String(binding.uav_counter_binding->binding) + ", ";
                str += "name=" + String(binding.uav_counter_binding->name);
                str += ");";
                str += "\n";
            }

            str += "       name    : " + String(binding.name);
            if ((binding.type_description->type_name != nullptr) && (strlen(binding.type_description->type_name) > 0))
            {
                str += " (" + String(binding.type_description->type_name) + ")";
            }
            str += "\n";
        }
    }

    str += "\n";
}


void Help()
{
    ErrorExit("Usage: SpirvShaderPacker -options <input spirvbytecode file> <output ushd file>\n"
              "\n"
              "Options:\n"
              ""
              "-debug show a debug log.\n"
              "-h Shows this help message.\n");
}


void Run(Vector<String>& arguments)
{
    if (arguments.Size() < 1)
        Help();

    SharedPtr<Context> context(new Context());
    context->RegisterSubsystem(new FileSystem(context));
    context->RegisterSubsystem(new Log(context));
    context->RegisterSubsystem(new Graphics(context));

    FileSystem* fileSystem = context->GetSubsystem<FileSystem>();
    Graphics* graphics = context->GetSubsystem<Graphics>();

    Log* log = context->GetSubsystem<Log>();
    log->SetLevel(LOG_TRACE);
    log->SetTimeStamp(false);

    Vector<String> inputFiles;
    String outputFile;
    String spriteSheetFileName;
    bool debug = false;
    bool help = false;

    while (arguments.Size() > 0)
    {
        String arg = arguments[0];
        arguments.Erase(0);

        if (arg.Empty())
            continue;

        if (arg.StartsWith("-"))
        {
            if (arg == "-h")
            {
                help = true;
                break;
            }
            else if (arg == "-debug")
            {
                debug = true;
            }
        }
        else
        {
            inputFiles.Push(arg);
        }
    }

    if (help)
        Help();

    String filename = GetFileName(inputFiles[0]);

    if (debug)
    {
        URHO3D_LOGTRACEF("entry file = %s", filename.CString());
    }

    // Take last input file as output
    if (inputFiles.Size() > 1)
    {
        outputFile = inputFiles[inputFiles.Size() - 1];
        if (outputFile.Length() > 2 && outputFile[0] != '/' && outputFile[1] != ':')
            outputFile = fileSystem->GetCurrentDir() + outputFile;

        if (debug)
            URHO3D_LOGTRACE("Output file set to " + outputFile + ".");

        inputFiles.Erase(inputFiles.Size() - 1);
    }

    // Check all input files exist
    for (unsigned i = 0; i < inputFiles.Size(); ++i)
    {
        if (!fileSystem->FileExists(inputFiles[i]))
            ErrorExit("File " + inputFiles[i] + " does not exist !");
    }

    // Collecting datas to save in urho shader file
    ShaderType type;
    uint64_t elementHash = 0;
    HashMap<StringHash, ShaderParameter> parameters;
    bool useTextureUnit[MAX_TEXTURE_UNITS];
    for (unsigned i=0; i < MAX_TEXTURE_UNITS; i++)
        useTextureUnit[i] = false;
    PODVector<uint32_t> byteCode;
    HashMap<unsigned, DescriptorsGroup> descriptorSets;

    // for debug
    String infostr;

    // Load Spirv Byte Code
    {
        SharedPtr<File> file(new File(context));
        if (!file->Open(inputFiles[0]))
        {
            URHO3D_LOGERROR(inputFiles[0] + " is not a valid spirv bytecode file !");
            return;
        }

        unsigned byteCodeSize = file->GetSize();
        byteCode.Resize(byteCodeSize);

        file->Seek(0);
        file->Read(byteCode.Buffer(), byteCodeSize);
        file->Close();
    }

    if (!byteCode.Size())
    {
        URHO3D_LOGERROR(inputFiles[0] + " has no bytecode !");
        return;
    }

    // Create Reflection Module
    SpvReflectShaderModule module = {};
    SpvReflectResult result = spvReflectCreateShaderModule(byteCode.Size(), byteCode.Buffer(), &module);
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        URHO3D_LOGERRORF("Can't create shader reflection module !");
        return;
    }

    if (debug)
        GetSpirvInfo(infostr, module);

    type = module.shader_stage == SPV_REFLECT_SHADER_STAGE_VERTEX_BIT ? VS : PS;

    String name = filename.Substring(0, filename.Find('.', 1));
    Vector<String> names = name.Split('_');
    String shadername = names[0];
    names.Erase(0);

    String defines;
    if (names.Size())
    {
        // normalized defines
        Sort(names.Begin(), names.End());
        defines = String::Joined(names, " ").ToUpper();
    }

    StringHash definehash;
    if (!defines.Empty())
        definehash = StringHash(defines);

    if (debug)
        URHO3D_LOGTRACEF("entry name=%s => shader=%s defines=%s => result=%s_%s",
                         name.CString(), shadername.CString(), defines.CString(), shadername.CString(), definehash.ToString().CString());

    if (outputFile.Empty())
    {
        String pathName, fileName, extension;
        SplitPath(inputFiles[0], pathName, fileName,  extension);
        outputFile = pathName + shadername + "_" + definehash.ToString() + (type == VS ? ".vs5" : ".ps5");
    }

    // Get vertex attributes
    if (type == VS)
    {
        // Get Input variables
        unsigned numInputVariables = 0;
        result = spvReflectEnumerateInputVariables(&module, &numInputVariables, nullptr);
        if (result != SPV_REFLECT_RESULT_SUCCESS)
        {
            URHO3D_LOGERRORF("Can't enumerate input variables !");
            return;
        }
        PODVector<SpvReflectInterfaceVariable*> inputVariables(numInputVariables);
        result = spvReflectEnumerateInputVariables(&module, &numInputVariables, inputVariables.Buffer());
        if (result != SPV_REFLECT_RESULT_SUCCESS)
        {
            URHO3D_LOGERRORF("Can't get input variables !");
            return;
        }

        if (debug)
            GetSpirvInfo(infostr, module.source_language, inputVariables);

        // Set Element Hash
        for (PODVector<SpvReflectInterfaceVariable*>::ConstIterator it=inputVariables.Begin(); it != inputVariables.End(); ++it)
        {
            const SpvReflectInterfaceVariable& obj = **it;

            VertexElementSemantic semantic = MAX_VERTEX_ELEMENT_SEMANTICS;
            String semanticName(obj.semantic != nullptr ? obj.semantic : obj.name);
            if (semanticName.Contains("pos", false))
                semantic = SEM_POSITION;
            else if (semanticName.Contains("binormal", false))
                semantic = SEM_BINORMAL;
            else if (semanticName.Contains("normal", false))
                semantic = SEM_NORMAL;
            else if (semanticName.Contains("tangent", false))
                semantic = SEM_TANGENT;
            else if (semanticName.Contains("texcoord", false) || semanticName.Contains("textcoord", false) )
                semantic = SEM_TEXCOORD;
            else if (semanticName.Contains("color", false))
                semantic = SEM_COLOR;
            else if (semanticName.Contains("blendwei", false))
                semantic = SEM_BLENDWEIGHTS;
            else if (semanticName.Contains("blendind", false))
                semantic = SEM_BLENDINDICES;
            else if (semanticName.Contains("objectind", false))
                semantic = SEM_OBJECTINDEX;

            unsigned location = obj.location;
            //String type       = ToStringType(src_lang, *obj.type_description);

            if (semantic != MAX_VERTEX_ELEMENT_SEMANTICS)
            {
                if (debug)
                    URHO3D_LOGTRACEF("stage VS : find location=%d semantic=%s(%u)", location, vertexElementSemanticNames[(int)semantic], (int)semantic);
                elementHash <<= 4;
                elementHash += ((int)semantic + 1) * (location + 1);
            }
        }

        elementHash <<= 32;

        if (debug)
            URHO3D_LOGTRACE("stage VS : element Hash=" + String(elementHash));
    }

    // Get samplers and parameters
    {
        // Get Descriptors Set info
        unsigned numDescriptorSets = 0;
        result = spvReflectEnumerateDescriptorSets(&module, &numDescriptorSets, nullptr);
        if (result != SPV_REFLECT_RESULT_SUCCESS)
        {
            URHO3D_LOGERRORF("Can't enumerate descriptor sets !");
            return;
        }
        PODVector<SpvReflectDescriptorSet*> spvSets(numDescriptorSets);
        result = spvReflectEnumerateDescriptorSets(&module, &numDescriptorSets, spvSets.Buffer());
        if (result != SPV_REFLECT_RESULT_SUCCESS)
        {
            URHO3D_LOGERRORF("Can't get descriptor sets !");
            return;
        }

        if (debug)
            GetSpirvInfo(infostr, spvSets);

        unsigned textureUnit = 0;
        for (PODVector<SpvReflectDescriptorSet*>::ConstIterator it=spvSets.Begin(); it != spvSets.End(); ++it)
        {
            const SpvReflectDescriptorSet& obj = **it;

            unsigned set = obj.set;

            DescriptorsGroup& descriptorSet = descriptorSets[set];
            descriptorSet.id_ = set;
            descriptorSet.bindings_.Resize(obj.binding_count);

            for (unsigned j = 0; j < obj.binding_count; ++j)
            {
                const SpvReflectDescriptorBinding& spvBinding = *obj.bindings[j];

                unsigned bind = spvBinding.binding;

                ShaderBind& binding = descriptorSet.bindings_[j];
                binding.id_ = bind;
                binding.type_ = (VkDescriptorType)spvBinding.descriptor_type;

                // Samplers
                if (binding.type_ == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                {
                    String varName(spvBinding.name);
                    if (varName[0] == 's')
                        varName = varName.Substring(1); // Strip the c to follow Urho3D constant naming convention

                    if (debug)
                        URHO3D_LOGTRACEF("set=%u bind=%u type=SAMPLER name=%s numSamplers=%u",
                                         set, bind, varName.CString(), spvBinding.count);

                    binding.unitStart_ = spvBinding.count == 1 ? GetStringListIndex(varName.CString(), textureUnitNames, 0, false) : 0;
                    binding.unitRange_ = spvBinding.count;

                    for (unsigned k = 0; k < binding.unitRange_; k++)
                        useTextureUnit[binding.unitStart_+k] = true;
                }
                // Input Attachments
                else if (binding.type_ == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
                {

                }
                // Uniform Buffers
                else if (binding.type_ == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || binding.type_ == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
                {
                    String groupName(spvBinding.type_description->type_name);
                    groupName = groupName.Substring(0, groupName.Length()-2); // Remove the tag VS or PS
                    unsigned char groupIndex = GetStringListIndex(groupName.CString(), parametersGroupNames, 0, false);

                    bool dynamic = ((groupIndex == SP_OBJECT && type == VS) || (groupIndex == SP_CAMERA && type == VS) || (groupIndex == SP_LIGHT && type == VS) || (groupIndex == SP_LIGHT && type == PS));
                    if (debug)
                        URHO3D_LOGTRACEF("set=%u bind=%u type=UNIFORM_BUFFER group=%s(%u) dynamic=%s", set, bind, spvBinding.type_description->type_name, groupIndex, dynamic?"true":"false");

                    const SpvReflectBlockVariable& block = spvBinding.block;
                    for (unsigned v=0; v < block.member_count; v++ )
                    {
                        const SpvReflectBlockVariable& var = block.members[v];
                        String varName(var.name);
                        if (varName[0] == 'c')
                            varName = varName.Substring(1); // Strip the c to follow Urho3D constant naming convention

                        ShaderParameter& parameter = parameters[varName];
                        parameter.type_   = type;
                        parameter.name_   = varName;
                        parameter.buffer_ = groupIndex;
                        parameter.offset_ = var.offset;
                        parameter.size_   = var.size;

                        if (debug)
                            URHO3D_LOGTRACEF("   offset=%u size=%u var=%s", var.offset, var.size, varName.CString());
                    }

                    binding.unitStart_ = groupIndex;
                    binding.unitRange_ = 1;

                    if (dynamic)
                        binding.type_ = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                }
            }
        }
    }

    // Print Infos
    if (debug)
        URHO3D_LOGINFO(infostr);

    spvReflectDestroyShaderModule(&module);

    // Save to Ouput File

    SharedPtr<File> file(new File(context, outputFile, FILE_WRITE));
    if (!file->IsOpen())
        return;

    file->WriteFileID("USHD");
    file->WriteShort((unsigned short)type);
    file->WriteShort(5);
    file->WriteUInt(elementHash >> 32);

    // Write Descriptors
    file->WriteUInt(descriptorSets.Size());
    for (HashMap<unsigned, DescriptorsGroup>::ConstIterator it = descriptorSets.Begin(); it != descriptorSets.End(); ++it)
    {
        const DescriptorsGroup& d = it->second_;

        file->WriteUByte((unsigned char)it->first_);
        file->WriteUByte((unsigned char)d.bindings_.Size());

        if (debug)
            URHO3D_LOGTRACEF("set=%u ...", it->first_);

        for (int i=0; i < d.bindings_.Size(); i++)
        {
            const ShaderBind& binding = d.bindings_[i];
            file->WriteUByte((unsigned char)binding.id_);
            file->WriteUByte((unsigned char)binding.type_);
            file->WriteUByte((unsigned char)binding.unitStart_);
            file->WriteUByte((unsigned char)binding.unitRange_);

            if (debug)
            {
                if (binding.type_ == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                    URHO3D_LOGTRACEF(" ... bind=%u type=%s(%d) unit=%u to %u", binding.id_, descriptorTypeNames[binding.type_],
                                     binding.type_, binding.unitStart_, binding.unitStart_+binding.unitRange_-1);
                else
                    URHO3D_LOGTRACEF(" ... bind=%u type=%s(%d) group=%u", binding.id_, descriptorTypeNames[binding.type_],
                                     binding.type_, binding.unitStart_);
            }
        }
    }

    // Write parameters
    file->WriteUInt(parameters.Size());
    for (HashMap<StringHash, ShaderParameter>::ConstIterator it = parameters.Begin(); it != parameters.End(); ++it)
    {
        file->WriteString(it->second_.name_);
        file->WriteUByte((unsigned char)it->second_.buffer_);
        file->WriteUInt(it->second_.offset_);
        file->WriteUInt(it->second_.size_);
    }

    // Write texture units
    unsigned usedTextureUnits = 0;
    for (unsigned i = 0; i < MAX_TEXTURE_UNITS; ++i)
    {
        if (useTextureUnit[i])
            ++usedTextureUnits;
    }
    file->WriteUInt(usedTextureUnits);
    for (unsigned i = 0; i < MAX_TEXTURE_UNITS; ++i)
    {
        if (useTextureUnit[i])
        {
            file->WriteString(graphics->GetTextureUnitName((TextureUnit)i));
            file->WriteUByte((unsigned char)i);
        }
    }

    file->WriteUInt(byteCode.Size());
    if (byteCode.Size())
        file->Write(&byteCode[0], byteCode.Size());

    file->Close();

    //URHO3D_LOGINFOF("Packed shader file %s ... OK !", outputFile.CString());
}

int main(int argc, char** argv)
{
    Vector<String> arguments;

#ifdef WIN32
    arguments = ParseArguments(GetCommandLineW());
#else
    arguments = ParseArguments(argc, argv);
#endif

    Run(arguments);
    return 0;
}

/*
    // Initialize reflection
//    std::vector<uint32_t> spirv_binary(k_sample_spv, k_sample_spv + sizeof(k_sample_spv));
//    spirv_cross::Compiler comp(std::move(spirv_binary));
    spirv_cross::Compiler comp(k_sample_spv, sizeof(k_sample_spv));

    // Get the shader resources (vertex attributes, uniforms, samplers)
    spirv_cross::ShaderResources res = comp.get_shader_resources();

    // Get vertex attributes
    unsigned elementHash_;
//    if (type == VS)
    {
        for (auto &resource : res.stage_inputs)
        {
            const spirv_cross::SPIRType& base_type = comp.get_type(resource.base_type_id);
            const spirv_cross::SPIRType& type      = comp.get_type(resource.type_id);
            unsigned location                      = comp.get_decoration(resource.id, spv::DecorationLocation);
            VertexElementSemantic semantic         = (VertexElementSemantic)GetStringListIndex(resource.name.c_str(), vertexElementSemanticNames, MAX_VERTEX_ELEMENT_SEMANTICS, true);
            if (semantic != MAX_VERTEX_ELEMENT_SEMANTICS)
            {
                elementHash_ <<= 4;
                elementHash_ += ((int)semantic + 1) * (location + 1);
            }
        }
        elementHash_ <<= 32;
    }

	// Get all sampled images in the shader.
	for (auto &resource : res.sampled_images)
	{
	    // Notice how we're using type_id here because we need the array information and not decoration information.
        const spirv_cross::SPIRType& type = comp.get_type(resource.type_id);
		unsigned set                      = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
		unsigned binding                  = comp.get_decoration(resource.id, spv::DecorationBinding);

		printf("Image %s at set = %u, binding = %u\n", resource.name.c_str(), set, binding);
        printf("%u", type.array.size()); // 1, because it's one dimension.
        printf("%u", type.array[0]); // 10
        printf("%u",type.array_size_literal[0]); // true
	}

    // Extract uniforms
    HashMap<StringHash, ShaderParameter> parameters;

    // Extract samplers
    bool useTextureUnit[MAX_TEXTURE_UNITS];

*/

// Set Parameters
//    for (unsigned i = 0; i < shaderDesc.ConstantBuffers; ++i)
//    {
//        ID3D11ShaderReflectionConstantBuffer* cb = reflection->GetConstantBufferByIndex(i);
//        D3D11_SHADER_BUFFER_DESC cbDesc;
//        cb->GetDesc(&cbDesc);
//        unsigned cbRegister = cbRegisterMap[String(cbDesc.Name)];
//
//        for (unsigned j = 0; j < cbDesc.Variables; ++j)
//        {
//            ID3D11ShaderReflectionVariable* var = cb->GetVariableByIndex(j);
//            D3D11_SHADER_VARIABLE_DESC varDesc;
//            var->GetDesc(&varDesc);
//            String varName(varDesc.Name);
//            if (varName[0] == 'c')
//            {
//                varName = varName.Substring(1); // Strip the c to follow Urho3D constant naming convention
//                ShaderParameter parameter;
//                parameter.type_ = type_;
//                parameter.name_ = varName;
//                parameter.buffer_ = cbRegister;
//                parameter.offset_ = varDesc.StartOffset;
//                parameter.size_ = varDesc.Size;
//                parameters_[varName] = parameter;
//            }
//        }
//    }

// Set Texture Units
//    HashMap<String, unsigned> cbRegisterMap;
//
//    for (unsigned i = 0; i < shaderDesc.BoundResources; ++i)
//    {
//        D3D11_SHADER_INPUT_BIND_DESC resourceDesc;
//        reflection->GetResourceBindingDesc(i, &resourceDesc);
//        String resourceName(resourceDesc.Name);
//        if (resourceDesc.Type == D3D_SIT_CBUFFER)
//            cbRegisterMap[resourceName] = resourceDesc.BindPoint;
//        else if (resourceDesc.Type == D3D_SIT_SAMPLER && resourceDesc.BindPoint < MAX_TEXTURE_UNITS)
//            useTextureUnit_[resourceDesc.BindPoint] = true;
//    }
