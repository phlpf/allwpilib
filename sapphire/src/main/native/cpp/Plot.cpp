#include "Plot.h"
#include "DataLog.h"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <implot.h>
#include <implot_internal.h>
#include "fmt/format.h"

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

void EntryPlot::CreatePlot(PlotAxis& axis, int startts, int endts, float sampleRate){
    points.clear();

    auto begin = m_entry->GetIterator(startts);
    auto end = m_entry->GetIterator(endts);
    auto it = begin;
    double& min = axis.min;
    double& max = axis.max;
    double lastTimestamp = endts;
    double lastValue = 0;
    while(it != end){
        auto record = it->second;
        auto value = GetValue(*m_entry, record);
        if(axis.autofit){
            if(value <= min){ 
                min = value; 
                min -= abs(max-min)/10;
                max += abs(max-min)/10;
            }
            if(value >= max){ 
                max = value + (value/10);
                min -= abs(max-min)/10;
                max += abs(max-min)/10;
            }
        }
        double timestamp = it->first*1e-6;
        double timesteps = (timestamp - lastTimestamp)/sampleRate;
        if(timestamp - lastTimestamp > sampleRate){ // if we have not changed for a significant amount of time
            points.emplace_back(ImPlotPoint{timestamp-sampleRate - m_offset, lastValue});
        }
        points.emplace_back(ImPlotPoint{timestamp - m_offset, value});
        ++it;
        lastValue = value;
        lastTimestamp = timestamp;
    }
}

void EntryPlot::EmitPlot(Plot& view){
    
    struct GetterData {
        double now;
        double zeroTime;
        std::vector<ImPlotPoint>& data;
    };
    GetterData getterData = {view.m_now, 0, points};
    auto getter = [](int idx, void* data) {
        auto d = static_cast<GetterData*>(data);
        
        // if (idx == d->size) {
        //     return ImPlotPoint{
        //         d->now - d->zeroTime,
        //         d->data[d->offset == 0 ? d->size - 1 : d->offset - 1].y};
        // }
        // ImPlotPoint* point;
        // if (d->offset + idx < d->size) {
        //     point = &d->data[d->offset + idx];
        // } else {
        //     point = &d->data[d->offset + idx - d->size];
        // }
        ImPlotPoint* point = &d->data[idx];
        return ImPlotPoint{point->x - d->zeroTime, point->y};
    };
    
    if (ImPlot::GetCurrentPlot()->YAxis(m_yAxis).Enabled) {
        ImPlot::SetAxis(ImAxis_Y1 + m_yAxis);
    } else {
        ImPlot::SetAxis(ImAxis_Y1);
    }
    ImPlot::PlotLineG(id.c_str(), getter, &getterData, getterData.data.size());
}

void Plot::EmitContextMenu(){
    if (ImGui::BeginMenu(m_name.c_str())) {
        if(ImGui::MenuItem("Display Settings", "", settings.m_settings)){
            settings.m_settings = !settings.m_settings;
        }
        if(ImGui::MenuItem("Display Legend", "", settings.m_legend)){
            settings.m_legend = !settings.m_legend;
        }
        EmitSettings();
        ImGui::EndMenu();
    }
}

EntryPlot::PlotAction EntryPlot::EmitSettings(){
    if(ImGui::CollapsingHeader(id.c_str())){
        ImGui::PushID(id.c_str());
        if(ImGui::Button("Delete")){
            ImGui::PopID();
            return ACTION_DELETE;
        }
        m_color.ColorEdit3(id.c_str());
        ImGui::PopID();
    }
    return ACTION_NOTHING;
}

void Plot::EmitSettings(){
    ImGui::PushID("Settings");
    if(!settings.m_autoheight){
        if(ImGui::InputInt("Height", &m_height)){
            if(m_height < 0){
                m_height = 0;
            }
        }
    }
    std::vector<int> removed_idxs;
    for(int i = 0; i < plots.size(); i++){
        auto& plot = plots[i];
        auto action = plot->EmitSettings();
        if(action == EntryPlot::ACTION_DELETE){
            removed_idxs.emplace_back(i);
        }
    }
    for(int i = removed_idxs.size()-1; i >= 0; i--){
        auto idx = removed_idxs[i];
        if(idx < plots.size()){
            plots.erase(plots.begin()+idx);
        }
    }

    ImGui::PopID();
}

void Plot::DragDropAccept(){
    if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EntryData")){
        auto source = *static_cast<EntryData**>(payload->Data);
        auto it =
            std::find_if(plots.begin(), plots.end(), [=](const auto& elem) {
                return elem->GetId() == source->GetName();
            });
        if(it == plots.end()){
            plots.emplace_back(std::make_unique<EntryPlot>(source, source->GetName(), 0, plots.size()) );
            NotifyChange();
        }
    }
}

void Plot::DragDropTarget(){
    if(ImPlot::BeginDragDropTargetPlot() ||
        ImPlot::BeginDragDropTargetLegend()){
        DragDropAccept();
        ImPlot::EndDragDropTarget();
    }
}

void Plot::EmitPlot(){
    
    if (ImPlot::BeginPlot(m_name.c_str(), ImVec2(-1, m_height))) {
    

        // setup x axis
        ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoMenus);
        double now = m_now;
        ImPlot::SetupAxisLimits(
            ImAxis_X1, now - settings.m_viewTime, now,
            ImGuiCond_Always);


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
            if (plot->m_color.GetColorFloat()[3] == IMPLOT_AUTO) {
                plot->m_color.SetColor(ImPlot::GetColormapColor(plot->number));
            }
            ImPlot::SetNextLineStyle(plot->m_color.GetColor());
            plot->EmitPlot(*this);
        }

        DragDropTarget();

        ImPlotPlot* plot = ImPlot::GetCurrentPlot();
        ImPlot::EndPlot();
    }
}
void EntryPlot::Update(Plot& view){
    auto& axis = view.m_axis[m_yAxis];
    int end = (m_offset * 1e6) + (view.m_now * 1e6);
    int start = m_offset * 1e6;
    CreatePlot(axis, start, end, view.m_sampleRate);
    axis.apply = true;
}

void EntryPlot::CheckForChange(Plot& view){
    if(m_offset != m_entry->GetOffset()){
        m_offset = m_entry->GetOffset();
        Update(view);
    }
}

void Plot::Display() {
    ImGui::PushID(m_name.c_str());

    ImGui::Text(m_name.c_str());
    EmitPlot();
    if(m_nowRef != m_now){
        m_now = m_nowRef;
        NotifyChange();
    }
    for(auto& plot : plots){
        plot->CheckForChange(*this);
    }

    ImGui::PopID();
}


void Plot::NotifyChange(){
    // Update all EntryPlot's
    for(auto& plot : plots){
        plot->Update(*this);
    }
    settings.m_viewTime = m_now;
}


void PlotView::EmitContextMenu(){
    if (ImGui::BeginPopupContextItem(m_name.c_str())) {
        if(ImGui::Button("Add Plot")){
            plots.emplace_back(std::make_unique<Plot>(m_now, fmt::format("Plot {}", plots.size()+1)));
        }
        for(auto& plot : plots){
            plot->EmitContextMenu();
        }
        ImGui::EndPopup();
    }        
}

void PlotView::Display(){
    EmitContextMenu();
    if(plots.size() == 0){
        if(ImGui::Button("Add Plot")){
            plots.emplace_back(std::make_unique<Plot>(m_now, fmt::format("Plot {}", plots.size()+1)));
        }
    }
    for(auto& plot : plots){
        plot->Display();
    }
}
