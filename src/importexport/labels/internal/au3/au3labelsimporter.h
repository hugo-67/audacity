/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include "modularity/ioc.h"
#include "context/iglobalcontext.h"

#include "../../ilabelsimporter.h"

namespace au::importexport {
class Au3LabelsImporter : public ILabelsImporter, public muse::Contextable
{
public:
    muse::ContextInject<context::IGlobalContext> globalContext = { this };

    Au3LabelsImporter(const muse::modularity::ContextPtr& ctx)
        : muse::Contextable(ctx) {}

    muse::Ret importData(const muse::io::path_t& filePath) override;
    std::vector<std::string> supportedExtensions() const override;
};
}
