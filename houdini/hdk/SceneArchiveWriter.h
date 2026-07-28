#pragma once

// Compatibility include retained for source checkouts that referenced the
// experimental HDK-local header. Scene archive ownership now belongs to HouIO.
#include <houio/SceneArchive.h>

namespace houio::hdk
{
    using SceneArchiveOptions = houio::SceneArchiveOptions;
    using SceneArchiveWriter = houio::SceneArchiveWriter;
}
