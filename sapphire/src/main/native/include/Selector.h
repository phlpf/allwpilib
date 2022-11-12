//
// Created by bagatelle on 5/21/22.
//

#pragma once

#ifndef ALLWPILIB_SELECTOR_H
#define ALLWPILIB_SELECTOR_H

#include "DataLog.h"
#include "glass/View.h"
#include "LogViewLoader.h"

namespace sapphire {
    
class Selector: public glass::View {
    public:
        void Display() override;
        std::vector<std::unique_ptr<DataLogModel> >& GetDataLogs();
        void AddLoader(LogViewLoader* loader) {
            this->loader = loader;
        }
    private:
        LogViewLoader* loader = nullptr;
        std::unique_ptr<DataLogModel>& GetNextModel();
        std::vector<std::unique_ptr<DataLogModel> > logs;
};
}  // namespace sapphire


#endif  // ALLWPILIB_SELECTOR_H
