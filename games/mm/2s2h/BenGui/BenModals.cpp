#include "BenModals.h"
#include <imgui.h>
#include <mutex>
#include <vector>
#include <string>
#include "UIWidgets.hpp"
#include "BenGui.hpp"

struct BenModal {
    std::string title_;
    std::string message_;
    std::string button1_;
    std::string button2_;
    std::function<void()> button1callback_;
    std::function<void()> button2callback_;
};
std::vector<BenModal> modals;

bool closePopup = false;

// Guards the `modals` queue and `closePopup` flag. Recursive because
// DrawElement invokes user-supplied button callbacks while holding the
// lock, and a callback may legitimately register another popup from
// inside one of those callbacks. Added so any future thread-pool worker
// that calls RegisterPopup can do so safely against the main-thread
// DrawElement. Mirrors the SohModalWindow guard added in #268.
static std::recursive_mutex modalsMutex;

void BenModalWindow::Draw() {
    if (!IsVisible()) {
        return;
    }
    DrawElement();
    // Sync up the IsVisible flag if it was changed by ImGui
    SyncVisibilityConsoleVariable();
}

void BenModalWindow::DrawElement() {
    std::lock_guard<std::recursive_mutex> lock(modalsMutex);
    if (modals.size() > 0) {
        BenModal curModal = modals.at(0);
        if (!ImGui::IsPopupOpen(curModal.title_.c_str())) {
            ImGui::OpenPopup(curModal.title_.c_str());
        }
        if (closePopup) {
            ImGui::CloseCurrentPopup();
            modals.erase(modals.begin());
            closePopup = false;
        }
        if (ImGui::BeginPopupModal(curModal.title_.c_str(), NULL,
                                   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize |
                                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                                       ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::Text("%s", curModal.message_.c_str());
            UIWidgets::PushStyleButton(THEME_COLOR);
            if (ImGui::Button(curModal.button1_.c_str())) {
                if (curModal.button1callback_ != nullptr) {
                    curModal.button1callback_();
                }
                ImGui::CloseCurrentPopup();
                modals.erase(modals.begin());
            }
            UIWidgets::PopStyleButton();
            if (curModal.button2_ != "") {
                ImGui::SameLine();
                UIWidgets::PushStyleButton(THEME_COLOR);
                if (ImGui::Button(curModal.button2_.c_str())) {
                    if (curModal.button2callback_ != nullptr) {
                        curModal.button2callback_();
                    }
                    ImGui::CloseCurrentPopup();
                    modals.erase(modals.begin());
                }
                UIWidgets::PopStyleButton();
            }
        }
        ImGui::EndPopup();
    }
}

void BenModalWindow::RegisterPopup(std::string title, std::string message, std::string button1, std::string button2,
                                   std::function<void()> button1callback, std::function<void()> button2callback) {
    std::lock_guard<std::recursive_mutex> lock(modalsMutex);
    modals.push_back({ title, message, button1, button2, button1callback, button2callback });
}

bool BenModalWindow::IsPopupOpen(std::string title) {
    std::lock_guard<std::recursive_mutex> lock(modalsMutex);
    return !modals.empty() && modals.at(0).title_ == title;
}

void BenModalWindow::DismissPopup() {
    std::lock_guard<std::recursive_mutex> lock(modalsMutex);
    closePopup = true;
}
