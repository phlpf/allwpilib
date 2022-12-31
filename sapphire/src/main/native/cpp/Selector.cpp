//
// Created by bagatelle on 5/21/22.
//

#include "Selector.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <portable-file-dialogs.h>
#include <wpi/DataLogReader.h>
#include <memory>
#include <vector>

#include "App.h"
#include "DataLog.h"
#include "wpi/fmt/raw_ostream.h"
#include "fmt/format.h"
using namespace sapphire;

std::unique_ptr<DataLogModel>& Selector::GetNextModel(){
  for(auto& log : m_logs){
    if(log->m_flags.IsLogActive == false){
      return log;
    }
  }
  auto& ref = m_logs.emplace_back(std::make_unique<DataLogModel>());
  return ref;
}

void Selector::Display() {
  static std::string logFile = "";
  static std::unique_ptr<pfd::open_file> logFileSelector;
  static std::string logFileMessage;

  static std::string videoFile = "";
  static std::unique_ptr<pfd::open_file> videoFileSelector;

  if(ImGui::Button("Load Log")) {
    logFileSelector = std::make_unique<pfd::open_file>(
        "Select Data Log", "",
        std::vector<std::string>{ "Data Log Files", "*.wpilog *.csv"},
        pfd::opt::none);
  }
  ImGui::SameLine();
  ImGui::TextUnformatted(logFile.c_str());
  ImGui::TextUnformatted(logFileMessage.c_str());
  
  // if(ImGui::Button("Load Video")) {
  //   videoFileSelector = std::make_unique<pfd::open_file>(
  //     "Select Video File", "",
  //     std::vector<std::string>{ "Video Files", "*.mp4 *.MP4 *.mov"},
  //     pfd::opt::none);
  // }
  // ImGui::SameLine();
  // ImGui::TextUnformatted(videoFile.c_str());

  if(logFileSelector && logFileSelector->ready(0)) {
    auto result = logFileSelector->result();
    if(!result.empty()) {
      logFile = result[0];
      auto& log = GetNextModel();
      bool success = log->LoadWPILog(result[0]);
      if(success) {
        logFileMessage = "Success";
        auto filename = log->m_rawFilename;
        // Make sure there are no name conflicts
        int number = 0;
        for(auto& otherLog : m_logs){
          if(filename == otherLog->m_rawFilename && otherLog != log){
              number++;
          }
        }
        if(number){
          log->m_filename = fmt::format("{}({})", filename, number);
        }
        if(m_loader){
          m_loader->AddDataLog(*log);
        }
      } else {
        logFileMessage = "Failure";
      }
    }
    logFileSelector.reset();
  }

  // if(videoFileSelector && videoFileSelector->ready(0)) {
  //   auto result = videoFileSelector->result();
  //   if(!result.empty()) {
  //     videoFile = result[0];
  //   }
  //   videoFileSelector.reset();
  // }

}

std::vector<std::unique_ptr<DataLogModel> >& Selector::GetDataLogs() {
  return m_logs;
}