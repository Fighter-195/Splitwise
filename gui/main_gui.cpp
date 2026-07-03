// Splitwise (C++) — Dear ImGui desktop front-end.
//
// This file is the GUI twin of cli/main.cpp: pure "view". Every real decision
// (splitting a bill, computing balances, settling up, persistence) is delegated
// to the exact same engine classes the CLI uses. The engine has no idea whether
// a terminal or an ImGui window is driving it.
//
// Rendering stack: Win32 window + Direct3D 9 + Dear ImGui. DX9 is used because
// it ships with Windows and links cleanly under MinGW with no extra downloads.

#include <d3d9.h>
#include <tchar.h>
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"

#include "../engine/Group.h"
#include "../engine/Ledger.h"
#include "../storage/CsvStore.h"

using namespace splitwise;

// ---------------------------------------------------------------------------
// Direct3D 9 scaffolding (adapted from the official ImGui Win32+DX9 example).
// ---------------------------------------------------------------------------

static LPDIRECT3D9        g_pD3D = nullptr;
static LPDIRECT3DDEVICE9  g_pd3dDevice = nullptr;
static bool               g_DeviceLost = false;
static UINT               g_ResizeWidth = 0, g_ResizeHeight = 0;
static D3DPRESENT_PARAMETERS g_d3dpp = {};

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg,
                                                             WPARAM wParam,
                                                             LPARAM lParam);

static bool CreateDeviceD3D(HWND hWnd) {
    if ((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == nullptr) return false;

    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;   // auto-detect
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;  // vsync on

    if (g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
                             D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp,
                             &g_pd3dDevice) < 0)
        return false;
    return true;
}

static void CleanupDeviceD3D() {
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
    if (g_pD3D) { g_pD3D->Release(); g_pD3D = nullptr; }
}

static void ResetDevice() {
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
    if (hr == D3DERR_INVALIDCALL) IM_ASSERT(0);
    ImGui_ImplDX9_CreateDeviceObjects();
}

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
        case WM_SIZE:
            if (wParam == SIZE_MINIMIZED) return 0;
            g_ResizeWidth = (UINT)LOWORD(lParam);
            g_ResizeHeight = (UINT)HIWORD(lParam);
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) return 0;  // disable ALT menu
            break;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Small helpers.
// ---------------------------------------------------------------------------

static std::string money(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.2f", v);
    return buf;
}

static std::string nameOf(const Group& g, int id) {
    const User* u = g.findUser(id);
    return u ? u->name() : ("#" + std::to_string(id));
}

// Copy a std::string into a fixed char buffer for ImGui::InputText.
static void setBuf(char* buf, size_t n, const std::string& s) {
    std::strncpy(buf, s.c_str(), n - 1);
    buf[n - 1] = '\0';
}

// ---------------------------------------------------------------------------
// GUI state that must persist across frames (ImGui is immediate-mode: the app
// owns all the widget state, ImGui just draws it).
// ---------------------------------------------------------------------------

struct AppState {
    Group group{"My Group"};

    char groupName[128] = "My Group";
    char filePath[260]  = "group.csv";
    std::string status;              // last action result, shown in a status bar
    bool        statusIsError = false;

    // "add member" form
    char newMemberName[128] = "";

    // "add expense" form
    char newDesc[128] = "";
    int  splitType   = 0;            // 0 = equal, 1 = exact, 2 = percent
    int  payerIndex  = 0;            // index into group.members()
    float totalAmount = 0.0f;
    std::vector<char>  partSel;      // per-member checkbox (0/1), index-aligned
    std::vector<float> partAmount;   // per-member exact amount
    std::vector<float> partPercent;  // per-member percentage

    void setStatus(const std::string& s, bool err = false) {
        status = s;
        statusIsError = err;
    }

    // Keep the per-member form vectors sized to the member list.
    void syncFormSizes() {
        size_t n = group.members().size();
        partSel.resize(n, 0);
        partAmount.resize(n, 0.0f);
        partPercent.resize(n, 0.0f);
        if (payerIndex >= (int)n) payerIndex = 0;
    }
};

// ---------------------------------------------------------------------------
// UI sections.
// ---------------------------------------------------------------------------

static void drawGroupBar(AppState& s) {
    ImGui::SeparatorText("Group");

    ImGui::SetNextItemWidth(220);
    if (ImGui::InputText("name", s.groupName, sizeof(s.groupName)))
        s.group.setName(s.groupName);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(220);
    ImGui::InputText("file", s.filePath, sizeof(s.filePath));

    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        try {
            CsvStore::save(s.group, s.filePath);
            s.setStatus(std::string("Saved to ") + s.filePath);
        } catch (const std::exception& ex) {
            s.setStatus(ex.what(), true);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Load")) {
        try {
            s.group = CsvStore::load(s.filePath);
            setBuf(s.groupName, sizeof(s.groupName), s.group.name());
            s.syncFormSizes();
            s.setStatus("Loaded '" + s.group.name() + "' — " +
                        std::to_string(s.group.members().size()) + " members, " +
                        std::to_string(s.group.expenses().size()) + " expenses");
        } catch (const std::exception& ex) {
            s.setStatus(ex.what(), true);
        }
    }
}

static void drawMembers(AppState& s) {
    ImGui::SeparatorText("Members");

    ImGui::SetNextItemWidth(220);
    ImGui::InputText("##member", s.newMemberName, sizeof(s.newMemberName));
    ImGui::SameLine();
    if (ImGui::Button("Add member")) {
        std::string name = s.newMemberName;
        if (name.empty()) {
            s.setStatus("member name cannot be empty", true);
        } else {
            int id = s.group.nextUserId();
            s.group.addMember(User(id, name));
            s.newMemberName[0] = '\0';
            s.syncFormSizes();
            s.setStatus("added " + name + " (id " + std::to_string(id) + ")");
        }
    }

    if (s.group.members().empty()) {
        ImGui::TextDisabled("(no members yet — add at least 2 to log an expense)");
        return;
    }
    if (ImGui::BeginTable("members", 2,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("id");
        ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        for (const User& u : s.group.members()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%d", u.id());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(u.name().c_str());
        }
        ImGui::EndTable();
    }
}

static void drawAddExpense(AppState& s) {
    ImGui::SeparatorText("Add expense");

    const std::vector<User>& members = s.group.members();
    if (members.size() < 2) {
        ImGui::TextDisabled("(add at least 2 members first)");
        return;
    }
    s.syncFormSizes();

    ImGui::SetNextItemWidth(220);
    ImGui::InputText("description", s.newDesc, sizeof(s.newDesc));

    // payer combo
    std::string payerLabel =
        members[s.payerIndex].name() + " (id " +
        std::to_string(members[s.payerIndex].id()) + ")";
    ImGui::SetNextItemWidth(220);
    if (ImGui::BeginCombo("paid by", payerLabel.c_str())) {
        for (int i = 0; i < (int)members.size(); ++i) {
            bool sel = (i == s.payerIndex);
            std::string lbl = members[i].name() + " (id " +
                              std::to_string(members[i].id()) + ")";
            if (ImGui::Selectable(lbl.c_str(), sel)) s.payerIndex = i;
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::TextUnformatted("split:");
    ImGui::SameLine(); ImGui::RadioButton("equal", &s.splitType, 0);
    ImGui::SameLine(); ImGui::RadioButton("exact amounts", &s.splitType, 1);
    ImGui::SameLine(); ImGui::RadioButton("percentages", &s.splitType, 2);

    if (s.splitType != 1) {  // exact derives the total from the amounts
        ImGui::SetNextItemWidth(160);
        ImGui::InputFloat("total", &s.totalAmount, 0.0f, 0.0f, "%.2f");
    }

    // participant picker (+ per-person value for exact/percent)
    ImGui::TextUnformatted("participants:");
    if (ImGui::BeginTable("participants", 2,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("who");
        ImGui::TableSetupColumn(s.splitType == 1 ? "amount"
                                : s.splitType == 2 ? "percent"
                                                   : "");
        ImGui::TableHeadersRow();
        for (int i = 0; i < (int)members.size(); ++i) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            bool checked = s.partSel[i] != 0;
            std::string cbLabel = members[i].name() + "##p" + std::to_string(i);
            if (ImGui::Checkbox(cbLabel.c_str(), &checked))
                s.partSel[i] = checked ? 1 : 0;

            ImGui::TableNextColumn();
            if (s.partSel[i] && s.splitType == 1) {
                ImGui::SetNextItemWidth(120);
                std::string id = "##amt" + std::to_string(i);
                ImGui::InputFloat(id.c_str(), &s.partAmount[i], 0.0f, 0.0f, "%.2f");
            } else if (s.partSel[i] && s.splitType == 2) {
                ImGui::SetNextItemWidth(120);
                std::string id = "##pct" + std::to_string(i);
                ImGui::InputFloat(id.c_str(), &s.partPercent[i], 0.0f, 0.0f, "%.1f");
            }
        }
        ImGui::EndTable();
    }

    if (ImGui::Button("Add expense")) {
        std::string desc = s.newDesc;
        if (desc.empty()) desc = "expense";
        for (char& c : desc) if (c == ',' || c == '\n' || c == '\r') c = ' ';
        int payer = members[s.payerIndex].id();

        std::vector<int> chosen;
        for (int i = 0; i < (int)members.size(); ++i)
            if (s.partSel[i]) chosen.push_back(members[i].id());

        if (chosen.empty()) {
            s.setStatus("pick at least one participant", true);
        } else if (s.splitType == 0) {  // equal
            if (s.totalAmount <= 0.0f) {
                s.setStatus("total must be positive", true);
            } else {
                s.group.addExpense(std::unique_ptr<Expense>(new EqualSplit(
                    desc, payer, s.totalAmount, chosen)));
                s.setStatus("added equal-split expense");
            }
        } else if (s.splitType == 1) {  // exact
            std::map<int, double> amounts;
            double sum = 0.0;
            for (int i = 0; i < (int)members.size(); ++i)
                if (s.partSel[i]) {
                    amounts[members[i].id()] = s.partAmount[i];
                    sum += s.partAmount[i];
                }
            if (sum <= 0.0) {
                s.setStatus("exact amounts must add up to a positive total", true);
            } else {
                s.group.addExpense(std::unique_ptr<Expense>(
                    new ExactSplit(desc, payer, amounts)));
                s.setStatus("added exact-split expense (total " + money(sum) + ")");
            }
        } else {  // percent
            std::map<int, double> percents;
            double sum = 0.0;
            for (int i = 0; i < (int)members.size(); ++i)
                if (s.partSel[i]) {
                    percents[members[i].id()] = s.partPercent[i];
                    sum += s.partPercent[i];
                }
            if (s.totalAmount <= 0.0f) {
                s.setStatus("total must be positive", true);
            } else if (std::fabs(sum - 100.0) > 0.01) {
                s.setStatus("percentages add up to " + money(sum) +
                                "%, not 100%",
                            true);
            } else {
                s.group.addExpense(std::unique_ptr<Expense>(new PercentSplit(
                    desc, payer, s.totalAmount, percents)));
                s.setStatus("added percent-split expense");
            }
        }
    }
}

static void drawExpenses(AppState& s) {
    ImGui::SeparatorText("Expenses");
    if (s.group.expenses().empty()) {
        ImGui::TextDisabled("(no expenses yet)");
        return;
    }
    if (ImGui::BeginTable("expenses", 4,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("description");
        ImGui::TableSetupColumn("total");
        ImGui::TableSetupColumn("type");
        ImGui::TableSetupColumn("paid by");
        ImGui::TableHeadersRow();
        for (const auto& e : s.group.expenses()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            // A tree node lets each expense expand to show per-person shares.
            bool open = ImGui::TreeNodeEx(e->description().c_str(),
                                          ImGuiTreeNodeFlags_SpanFullWidth);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(money(e->total()).c_str());
            ImGui::TableNextColumn(); ImGui::TextUnformatted(e->type().c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(nameOf(s.group, e->paidBy()).c_str());
            if (open) {
                for (int pid : e->participants()) {
                    ImGui::BulletText("%s owes %s", nameOf(s.group, pid).c_str(),
                                      money(e->shareFor(pid)).c_str());
                }
                ImGui::TreePop();
            }
        }
        ImGui::EndTable();
    }
}

static void drawBalancesAndSettle(AppState& s) {
    Ledger ledger(s.group);

    ImGui::SeparatorText("Balances");
    std::map<int, double> bal = ledger.balances();
    bool any = false;
    for (const auto& kv : bal) {
        if (std::fabs(kv.second) < 0.005) continue;
        any = true;
        if (kv.second > 0)
            ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.45f, 1.0f), "%s is owed %s",
                               nameOf(s.group, kv.first).c_str(),
                               money(kv.second).c_str());
        else
            ImGui::TextColored(ImVec4(0.90f, 0.55f, 0.45f, 1.0f), "%s owes %s",
                               nameOf(s.group, kv.first).c_str(),
                               money(-kv.second).c_str());
    }
    if (!any) ImGui::TextDisabled("all settled up");

    ImGui::SeparatorText("Settle up (minimal payments)");
    std::vector<Payment> plan = ledger.settleUp();
    if (plan.empty()) {
        ImGui::TextDisabled("nothing to settle");
    } else {
        for (const Payment& p : plan) {
            ImGui::Text("%s  ->  %s : %s", nameOf(s.group, p.from).c_str(),
                        nameOf(s.group, p.to).c_str(), money(p.amount).c_str());
        }
    }
}

static void drawUI(AppState& s) {
    // One full-window panel docked to the host window.
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::Begin("Splitwise", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings);

    drawGroupBar(s);

    // Two columns: left = inputs, right = results.
    if (ImGui::BeginTable("layout", 2, ImGuiTableFlags_Resizable)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        drawMembers(s);
        drawAddExpense(s);
        ImGui::TableNextColumn();
        drawExpenses(s);
        drawBalancesAndSettle(s);
        ImGui::EndTable();
    }

    // status bar
    ImGui::Separator();
    if (!s.status.empty()) {
        ImVec4 col = s.statusIsError ? ImVec4(0.95f, 0.5f, 0.4f, 1.0f)
                                     : ImVec4(0.6f, 0.8f, 1.0f, 1.0f);
        ImGui::TextColored(col, "%s", s.status.c_str());
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------
// Entry point.
// ---------------------------------------------------------------------------

int main(int, char**) {
    // Window.
    WNDCLASSEX wc = {sizeof(wc),        CS_CLASSDC, WndProc, 0L,   0L,
                     GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr,
                     _T("Splitwise"),   nullptr};
    ::RegisterClassEx(&wc);
    HWND hwnd = ::CreateWindow(wc.lpszClassName, _T("Splitwise (C++)"),
                               WS_OVERLAPPEDWINDOW, 100, 100, 1100, 720, nullptr,
                               nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // ImGui.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // don't litter an imgui.ini next to the exe
    ImGui::StyleColorsDark();
    ImGui::GetStyle().FrameRounding = 4.0f;
    ImGui::GetStyle().WindowRounding = 0.0f;

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(g_pd3dDevice);

    AppState app;
    app.syncFormSizes();
    const ImVec4 clear(0.10f, 0.11f, 0.13f, 1.0f);

    bool done = false;
    while (!done) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        // Handle a pending resize requested from WndProc.
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
            g_d3dpp.BackBufferWidth = g_ResizeWidth;
            g_d3dpp.BackBufferHeight = g_ResizeHeight;
            g_ResizeWidth = g_ResizeHeight = 0;
            ResetDevice();
        }

        if (g_DeviceLost) {
            HRESULT hr = g_pd3dDevice->TestCooperativeLevel();
            if (hr == D3DERR_DEVICELOST) { ::Sleep(10); continue; }
            if (hr == D3DERR_DEVICENOTRESET) ResetDevice();
            g_DeviceLost = false;
        }

        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        drawUI(app);

        ImGui::EndFrame();
        g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        D3DCOLOR clearCol = D3DCOLOR_RGBA(
            (int)(clear.x * 255), (int)(clear.y * 255), (int)(clear.z * 255),
            (int)(clear.w * 255));
        g_pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                            clearCol, 1.0f, 0);
        if (g_pd3dDevice->BeginScene() >= 0) {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_pd3dDevice->EndScene();
        }
        HRESULT result = g_pd3dDevice->Present(nullptr, nullptr, nullptr, nullptr);
        if (result == D3DERR_DEVICELOST) g_DeviceLost = true;
    }

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);
    return 0;
}
