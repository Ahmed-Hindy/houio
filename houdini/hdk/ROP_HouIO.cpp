#include "ROP_HouIO.h"

#include "HoudiniGeometryAdapter.h"

#include <CH/CH_LocalVariable.h>
#include <GU/GU_Detail.h>
#include <OP/OP_OperatorTable.h>
#include <PRM/PRM_Include.h>
#include <PRM/PRM_SpareData.h>
#include <ROP/ROP_Error.h>
#include <ROP/ROP_Templates.h>
#include <SOP/SOP_Node.h>
#include <UT/UT_DSOVersion.h>
#include <UT/UT_Interrupt.h>

#include <houio/NativePolygonWriter.h>

#include <array>
#include <cstdlib>
#include <stdexcept>
#include <string>

using namespace houio::hdk;

namespace
{
    enum ParameterIndex
    {
        parameter_render,
        parameter_render_control,
        parameter_switcher,
        parameter_time_range,
        parameter_frame_range,
        parameter_take,
        parameter_sop_path,
        parameter_output,
        parameter_create_directories,
        parameter_overwrite,
        parameter_atomic,
        parameter_pre_render_language,
        parameter_pre_render,
        parameter_pre_render_enable,
        parameter_pre_frame_language,
        parameter_pre_frame,
        parameter_pre_frame_enable,
        parameter_post_frame_language,
        parameter_post_frame,
        parameter_post_frame_enable,
        parameter_post_render_language,
        parameter_post_render,
        parameter_post_render_enable,
        parameter_count
    };

    enum IndirectIndex
    {
        indirect_sop_path,
        indirect_output,
        indirect_create_directories,
        indirect_overwrite,
        indirect_atomic,
        indirect_count
    };

    PRM_Name switcher_name("houio_tabs");
    PRM_Default switcher_defaults[] = {
        PRM_Default(8, "Export"),
        PRM_Default(12, "Scripts"),
    };

    PRM_Name sop_path_name("soppath", "SOP Path");
    PRM_Name output_name("sopoutput", "Output File");
    PRM_Default output_default(0, "$HIP/geo/$OS.$F4.bgeo.sc");
    PRM_Name create_directories_name("createdirs", "Create Intermediate Directories");
    PRM_Name overwrite_name("overwrite", "Overwrite Existing Files");
    PRM_Name atomic_name("atomic", "Atomic Write");

    PRM_Template custom_templates[] = {
        PRM_Template(
            PRM_STRING,
            PRM_TYPE_DYNAMIC_PATH,
            1,
            &sop_path_name,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            &PRM_SpareData::sopPath),
        PRM_Template(
            PRM_FILE,
            1,
            &output_name,
            &output_default,
            nullptr,
            nullptr,
            nullptr,
            &PRM_SpareData::fileChooserModeWrite),
        PRM_Template(PRM_TOGGLE, 1, &create_directories_name, PRMoneDefaults),
        PRM_Template(PRM_TOGGLE, 1, &overwrite_name, PRMoneDefaults),
        PRM_Template(PRM_TOGGLE, 1, &atomic_name, PRMoneDefaults),
        PRM_Template()
    };

    PRM_Template* getTemplates()
    {
        static PRM_Template* templates = nullptr;
        if (templates != nullptr)
            return templates;

        templates = new PRM_Template[parameter_count + 1];
        templates[parameter_render] = theRopTemplates[ROP_RENDER_TPLATE];
        templates[parameter_render_control] = theRopTemplates[ROP_RENDERDIALOG_TPLATE];
        templates[parameter_switcher] = PRM_Template(
            PRM_SWITCHER,
            static_cast<int>(std::size(switcher_defaults)),
            &switcher_name,
            switcher_defaults);
        templates[parameter_time_range] = theRopTemplates[ROP_TRANGE_TPLATE];
        templates[parameter_frame_range] = theRopTemplates[ROP_FRAMERANGE_TPLATE];
        templates[parameter_take] = theRopTemplates[ROP_TAKENAME_TPLATE];
        templates[parameter_sop_path] = custom_templates[0];
        templates[parameter_output] = custom_templates[1];
        templates[parameter_create_directories] = custom_templates[2];
        templates[parameter_overwrite] = custom_templates[3];
        templates[parameter_atomic] = custom_templates[4];
        templates[parameter_pre_render_language] = theRopTemplates[ROP_TPRERENDER_TPLATE];
        templates[parameter_pre_render] = theRopTemplates[ROP_PRERENDER_TPLATE];
        templates[parameter_pre_render_enable] = theRopTemplates[ROP_LPRERENDER_TPLATE];
        templates[parameter_pre_frame_language] = theRopTemplates[ROP_TPREFRAME_TPLATE];
        templates[parameter_pre_frame] = theRopTemplates[ROP_PREFRAME_TPLATE];
        templates[parameter_pre_frame_enable] = theRopTemplates[ROP_LPREFRAME_TPLATE];
        templates[parameter_post_frame_language] = theRopTemplates[ROP_TPOSTFRAME_TPLATE];
        templates[parameter_post_frame] = theRopTemplates[ROP_POSTFRAME_TPLATE];
        templates[parameter_post_frame_enable] = theRopTemplates[ROP_LPOSTFRAME_TPLATE];
        templates[parameter_post_render_language] = theRopTemplates[ROP_TPOSTRENDER_TPLATE];
        templates[parameter_post_render] = theRopTemplates[ROP_POSTRENDER_TPLATE];
        templates[parameter_post_render_enable] = theRopTemplates[ROP_LPOSTRENDER_TPLATE];
        templates[parameter_count] = PRM_Template();
        UT_ASSERT(PRM_Template::countTemplates(templates) == parameter_count);
        return templates;
    }
}

int* ROP_HouIO::indirect_ = nullptr;

OP_TemplatePair* ROP_HouIO::getTemplatePair()
{
    static OP_TemplatePair* pair = nullptr;
    if (pair == nullptr)
        pair = new OP_TemplatePair(getTemplates());
    return pair;
}

OP_VariablePair* ROP_HouIO::getVariablePair()
{
    static OP_VariablePair* pair = nullptr;
    if (pair == nullptr)
        pair = new OP_VariablePair(ROP_Node::myVariableList);
    return pair;
}

OP_Node* ROP_HouIO::myConstructor(
    OP_Network* network,
    const char* name,
    OP_Operator* operator_entry)
{
    return new ROP_HouIO(network, name, operator_entry);
}

ROP_HouIO::ROP_HouIO(
    OP_Network* network,
    const char* name,
    OP_Operator* operator_entry)
    : ROP_Node(network, name, operator_entry)
{
    if (indirect_ == nullptr)
        indirect_ = allocIndirect(indirect_count);
}

void ROP_HouIO::evaluateString(
    UT_String& value,
    const char* name,
    int index,
    fpreal time)
{
    evalString(value, name, &indirect_[index], 0, time);
}

bool ROP_HouIO::evaluateToggle(const char* name, int index, fpreal time)
{
    return evalInt(name, &indirect_[index], 0, time) != 0;
}

int ROP_HouIO::startRender(int, fpreal start_time, fpreal end_time)
{
    end_time_ = end_time;
    if (error() < UT_ERROR_ABORT && !executePreRenderScript(start_time))
        return 0;
    return error() < UT_ERROR_ABORT ? 1 : 0;
}

ROP_RENDER_CODE ROP_HouIO::renderFrame(fpreal time, UT_Interrupt* interrupt)
{
    if (!executePreFrameScript(time))
        return ROP_ABORT_RENDER;

    try
    {
        if (interrupt != nullptr && interrupt->opInterrupt())
            return ROP_ABORT_RENDER;

        UT_String sop_path;
        evaluateString(sop_path, "soppath", indirect_sop_path, time);
        if (!sop_path.isstring())
        {
            addError(ROP_MESSAGE, "SOP Path is empty");
            return ROP_ABORT_RENDER;
        }

        SOP_Node* sop = getSOPNode(sop_path, 1);
        if (sop == nullptr)
        {
            addError(ROP_COOK_ERROR, sop_path.c_str());
            return ROP_ABORT_RENDER;
        }

        OP_Context context(time);
        GU_DetailHandle detail_handle = sop->getCookedGeoHandle(context);
        GU_DetailHandleAutoReadLock detail_lock(detail_handle);
        const GU_Detail* detail = detail_lock.getGdp();
        if (detail == nullptr)
        {
            addError(ROP_COOK_ERROR, sop_path.c_str());
            return ROP_ABORT_RENDER;
        }

        UT_String output_path;
        evaluateString(output_path, "sopoutput", indirect_output, time);
        if (!output_path.isstring())
        {
            addError(ROP_MESSAGE, "Output File is empty");
            return ROP_ABORT_RENDER;
        }

        const NativePolygonDetail geometry = adaptDetail(*detail, interrupt);
        const std::string destination = output_path.toStdString();
        const char* blosc_library = std::getenv("HOUIO_BLOSC_LIBRARY");

        HouIONativePolygonWriteRequest request = {};
        request.positions_xyzw = geometry.positions_xyzw.empty()
            ? nullptr
            : geometry.positions_xyzw.data();
        request.point_count = geometry.positions_xyzw.size() / 4;
        request.topology = geometry.topology.empty()
            ? nullptr
            : geometry.topology.data();
        request.vertex_count = geometry.topology.size();
        request.polygons = geometry.polygons.empty()
            ? nullptr
            : geometry.polygons.data();
        request.polygon_count = geometry.polygons.size();
        request.destination_utf8 = destination.c_str();
        request.blosc_library_utf8 = blosc_library;
        request.create_parent_directories = evaluateToggle(
            "createdirs", indirect_create_directories, time)
            ? 1U
            : 0U;
        request.overwrite_existing = evaluateToggle(
            "overwrite", indirect_overwrite, time)
            ? 1U
            : 0U;
        request.atomic_replace = evaluateToggle(
            "atomic", indirect_atomic, time)
            ? 1U
            : 0U;

        std::array<char, 4096> error_buffer = {};
        const int write_status = houio_write_native_polygons(
            &request,
            error_buffer.data(),
            error_buffer.size());
        if (write_status != 0)
        {
            addError(
                ROP_MESSAGE,
                error_buffer[0] != '\0'
                    ? error_buffer.data()
                    : "HouIO native polygon writer failed without a diagnostic");
            return ROP_ABORT_RENDER;
        }
    }
    catch (const std::exception& exception)
    {
        addError(ROP_MESSAGE, exception.what());
        return ROP_ABORT_RENDER;
    }

    if (error() < UT_ERROR_ABORT && !executePostFrameScript(time))
        return ROP_ABORT_RENDER;
    return error() < UT_ERROR_ABORT ? ROP_CONTINUE_RENDER : ROP_ABORT_RENDER;
}

ROP_RENDER_CODE ROP_HouIO::endRender()
{
    if (error() < UT_ERROR_ABORT && !executePostRenderScript(end_time_))
        return ROP_ABORT_RENDER;
    return error() < UT_ERROR_ABORT ? ROP_CONTINUE_RENDER : ROP_ABORT_RENDER;
}

void ROP_HouIO::getOutputFile(UT_String& output)
{
    evaluateString(output, "sopoutput", indirect_output, CHgetEvalTime());
}

void newDriverOperator(OP_OperatorTable* table)
{
    table->addOperator(new OP_Operator(
        "houio::geometry",
        "HouIO Geometry",
        ROP_HouIO::myConstructor,
        ROP_HouIO::getTemplatePair(),
        0,
        0,
        ROP_HouIO::getVariablePair(),
        OP_FLAG_GENERATOR));
}
