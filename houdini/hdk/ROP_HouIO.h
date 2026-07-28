#pragma once

#include "SceneArchiveWriter.h"

#include <ROP/ROP_Node.h>

#include <memory>
#include <string>

class OP_TemplatePair;
class OP_VariablePair;

namespace houio::hdk
{
    class ROP_HouIO final : public ROP_Node
    {
    public:
        static OP_TemplatePair* getTemplatePair();
        static OP_VariablePair* getVariablePair();
        static OP_Node* myConstructor(
            OP_Network* network,
            const char* name,
            OP_Operator* operator_entry);

    protected:
        ROP_HouIO(
            OP_Network* network,
            const char* name,
            OP_Operator* operator_entry);
        ~ROP_HouIO() override;

        int startRender(int frame_count, fpreal start_time, fpreal end_time) override;
        ROP_RENDER_CODE renderFrame(fpreal time, UT_Interrupt* interrupt) override;
        ROP_RENDER_CODE endRender() override;
        void getOutputFile(UT_String& output) override;

    private:
        void evaluateString(UT_String& value, const char* name, int index, fpreal time);
        [[nodiscard]] bool evaluateToggle(const char* name, int index, fpreal time);

        static int* indirect_;
        std::unique_ptr<SceneArchiveWriter> archive_writer_;
        std::string archive_destination_;
        NativeOutputFormat render_format_ = NativeOutputFormat::unsupported;
        fpreal end_time_ = 0.0;
    };
}
