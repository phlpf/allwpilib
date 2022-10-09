#include "Plot.h"
#include "DataLog.h"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <implot.h>
#include <implot_internal.h>

#include <wpi/timestamp.h>
using namespace sapphire;

double GetValue(EntryData& data, wpi::log::DataLogRecord record){
    if(data.type == "double"){
        double val;
        if (record.GetDouble(&val)) {
            return val;
        }
    } else if(data.type == "int64"){
        int64_t val;
        if (record.GetInteger(&val)) {
            return (double)val;
        }
    }
    return 0;
}

void EntryPlot::CreatePlot(PlotAxis axis){
    auto begin = m_entry->GetIterator(axis.min);
    auto end = m_entry->GetIterator(axis.max);
    auto it = begin;
    double& min = axis.min;
    double& max = axis.max;
    int i = 0;
    int size = 0;
    while(it != end){
        auto record = it->second;
        auto value = GetValue(*m_entry, record);
        if(value < min){ min = value; }
        if(value > max){ max = value; }
        points[i] = ImPlotPoint{it->first * 1e-6, value};
        ++it;
        i = (i + 1) % kMaxSize;
        size++;
    }
    if(i < size){
        m_offset = i;
    } else {
        m_offset = 0;
    }
    m_size = std::min(size, kMaxSize);
}

void EntryPlot::EmitPlot(PlotView& view){
    
    struct GetterData {
        double now;
        double zeroTime;
        ImPlotPoint* data;
        int size;
        int offset;
    };
    GetterData getterData = {view.m_now, 0, points, kMaxSize, m_offset};
    auto getter = [](int idx, void* data) {
        auto d = static_cast<GetterData*>(data);
        if (idx == d->size) {
            return ImPlotPoint{
                d->now - d->zeroTime,
                d->data[d->offset == 0 ? d->size - 1 : d->offset - 1].y};
        }
        ImPlotPoint* point;
        if (d->offset + idx < d->size) {
            point = &d->data[d->offset + idx];
        } else {
            point = &d->data[d->offset + idx - d->size];
        }
        return ImPlotPoint{point->x - d->zeroTime, point->y};
    };
    
    if (ImPlot::GetCurrentPlot()->YAxis(m_yAxis).Enabled) {
        ImPlot::SetAxis(ImAxis_Y1 + m_yAxis);
    } else {
        ImPlot::SetAxis(ImAxis_Y1);
    }
    ImPlot::PlotLineG(id.c_str(), getter, &getterData, getterData.size + 1);
}

void PlotView::EmitContextMenu(){
    if (ImGui::BeginPopupContextItem(m_name.c_str())) {
        if(ImGui::MenuItem("Display Settings", "", settings.m_settings)){
            settings.m_settings = !settings.m_settings;
        }
        if(ImGui::MenuItem("Display Legend", "", settings.m_legend)){
            settings.m_legend = !settings.m_legend;
        }
    }
}
void PlotView::EmitSettings(){
    if(!settings.m_autoheight){
        if(ImGui::InputInt("Height", &m_height)){
            if(m_height < 0){
                m_height = 0;
            }
        }
    }
}

void PlotView::DragDropAccept(){
    if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EntryData")){
        auto source = *static_cast<EntryData**>(payload->Data);
        auto it =
            std::find_if(plots.begin(), plots.end(), [=](const auto& elem) {
                return elem->GetId() == source->GetName();
            });
        if(it == plots.end()){
            plots.emplace_back(std::make_unique<EntryPlot>(source, source->GetName(), 0) );
        }
    }
}

void PlotView::DragDropTarget(){
    if(ImPlot::BeginDragDropTargetPlot() ||
        ImPlot::BeginDragDropTargetLegend()){
        DragDropAccept();
        ImPlot::EndDragDropTarget();
    }
}

void PlotView::EmitPlot(){
    ImGui::PushID(m_name.c_str());
    EmitContextMenu();
    if(settings.m_settings){ EmitSettings(); }

    if (ImPlot::BeginPlot(m_name.c_str(), ImVec2(-1, m_height))) {
    

        // setup x axis
        ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoMenus);
        double now = m_now*1.0e-6;
        ImPlot::SetupAxisLimits(
            ImAxis_X1, now - settings.m_viewTime, now,
            ImGuiCond_Once);


        // setup y axes
        for (int i = 0; i < m_axis.size(); ++i) {
            ImPlotAxisFlags flags = 0;
                //   (m_axis[i].lockMin ? ImPlotAxisFlags_LockMin : 0) |
                //   (m_axis[i].lockMax ? ImPlotAxisFlags_LockMax : 0) |
                //   (m_axis[i].autoFit ? ImPlotAxisFlags_AutoFit : 0) |
                //   (m_axis[i].invert ? ImPlotAxisFlags_Invert : 0) |
                //   (m_axis[i].opposite ? ImPlotAxisFlags_Opposite : 0) |
                //   (m_axis[i].gridLines ? 0 : ImPlotAxisFlags_NoGridLines) |
                //   (m_axis[i].tickMarks ? 0 : ImPlotAxisFlags_NoTickMarks) |
                //   (m_axis[i].tickLabels ? 0 : ImPlotAxisFlags_NoTickLabels);
            ImPlot::SetupAxis(
                ImAxis_Y1 + i,
                m_axis[i].label.empty() ? nullptr : m_axis[i].label.c_str(), flags);
            ImPlot::SetupAxisLimits(
                ImAxis_Y1 + i, m_axis[i].min, m_axis[i].max,
                m_axis[i].apply ?
                ImGuiCond_Always : ImGuiCond_Once
                );
            ImPlot::SetupAxisScale(ImAxis_Y1 + i, m_axis[i].logScale
                                                        ? ImPlotScale_Log10 :  ImPlotScale_Linear);
            m_axis[i].apply = false;
        }

        ImPlot::SetupFinish();

        for(auto& plot : plots){
            ImPlot::SetNextLineStyle(plot->m_color);
            plot->EmitPlot(*this);
        }

        DragDropTarget();

        ImPlotPlot* plot = ImPlot::GetCurrentPlot();
        ImPlot::EndPlot();
    }
    ImGui::PopID();
}
void EntryPlot::Update(PlotView& view){
    auto& axis = view.m_axis[m_yAxis];
    CreatePlot(axis);
}

void PlotView::Display() {
    ImGui::Text("Plot View");
    if(ImGui::Button("Update")){
        NotifyChange();
    }
    EmitPlot();
}


void PlotView::NotifyChange(){
    // Update all EntryPlot's
    for(auto& plot : plots){
        plot->Update(*this);
    }
}
