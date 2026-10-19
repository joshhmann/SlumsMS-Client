#include "Drawing.h"
#include "Hook.h"
#include <chrono>
#include "../Funcs.h"
#include "../Util/MemPatch.h"
#include <intrin.h>
#include "../MapleClientCollectionTypes/ZXString.h"

BOOL Drawing::bInit = FALSE; // Status of the initialization of ImGui.
bool Drawing::bDisplay = false; // Status of the menu display.
bool Drawing::bSetPos = false; // Status to update ImGui window size / position.
ImVec2 Drawing::vWindowPos = { 0, 0 }; // Last ImGui window position.
ImVec2 Drawing::vWindowSize = { 0, 0 }; // Last ImGui window size.


//CUserLocal Address: 0xBEBF98
// Define the typedef for the function signature with 'void' instead of 'CUserLocal'
typedef void(__thiscall* SetDirectionMode_t)(void* pThis, int bSet);

// Define a static variable for the function pointer at the address
static SetDirectionMode_t SetDirectionMode = (SetDirectionMode_t)0x0095FF5A;
static void** INSTANCE_CUSERLOCAL= (void**)0x00BEBF98;


// Function to convert ARGB8 to float array for ImGui
void ARGB8ToFloatArray(uint32_t argb, float* color)
{
	color[0] = ((argb & 0x00FF0000) >> 16) / 255.0f; // Red
	color[1] = ((argb & 0x0000FF00) >> 8) / 255.0f;  // Green
	color[2] = (argb & 0x000000FF) / 255.0f;          // Blue
	color[3] = ((argb & 0xFF000000) >> 24) / 255.0f; // Alpha
}

// Function to convert float array back to ARGB8
uint32_t FloatArrayToARGB8(const float* color)
{
	uint32_t argb = 0;
	argb |= (static_cast<uint32_t>(color[3] * 255) << 24); // Alpha
	argb |= (static_cast<uint32_t>(color[0] * 255) << 16); // Red
	argb |= (static_cast<uint32_t>(color[1] * 255) << 8);  // Green
	argb |= (static_cast<uint32_t>(color[2] * 255));        // Blue
	return argb;
}

bool RenderColorPicker(uint32_t& argbColor)
{
	float color[4]; // RGBA
	ARGB8ToFloatArray(argbColor, color);

	bool colorChanged = ImGui::ColorEdit4("Color Picker", color);

	// If color is edited, convert it back to ARGB8
	if (colorChanged)
	{
		argbColor = FloatArrayToARGB8(color);
	}

	return colorChanged;
}

bool Drawing::Hook_CItemInfo__GetItemDesc(bool enable)
{
	typedef ZXString<char>* (__fastcall* CItemInfo__GetItemDesc_t)(void* pThis, void* edx, ZXString<char>* result, int nItemID);
	static auto CItemInfo__GetItemDesc = reinterpret_cast<CItemInfo__GetItemDesc_t>(0x005CF69E);

	CItemInfo__GetItemDesc_t hook = [](void* pThis, void* edx, ZXString<char>* result, int nItemID) -> ZXString<char>*
		{
			auto ret = CItemInfo__GetItemDesc(pThis, edx, result, nItemID);

			int type = nItemID / 1000000;
			if (type >= 1 && type <= 5)
			{
				if (ret->Length() > 0)
				{
					*ret += "\r\n";
				}
				*ret += "#cItem ID: ";
				*ret += std::to_string(nItemID).c_str();
				*ret += "#";
			}

			return ret;
		};
	return SetHook(enable, reinterpret_cast<void**>(&CItemInfo__GetItemDesc), hook);
}


void Drawing::draw() {
	ImGui::Begin("YourServer's Widget", &bDisplay);
	{
		ImGui::SetWindowSize({ 500, 300 }, ImGuiCond_Once);

		if (vWindowPos.x != 0.0f && vWindowPos.y != 0.0f && vWindowSize.x != 0.0f && vWindowSize.y != 0.0f && bSetPos)
		{
			ImGui::SetWindowPos(vWindowPos);
			ImGui::SetWindowSize(vWindowSize);
			bSetPos = false;
		}

		if (!bSetPos)
		{
			vWindowPos = { ImGui::GetWindowPos().x, ImGui::GetWindowPos().y };
			vWindowSize = { ImGui::GetWindowSize().x, ImGui::GetWindowSize().y };
		}

		ImGui::Text("Menu");
		ImGui::Separator();

		if (ImGui::BeginTabBar("Tabs")) {
			// QoL Tab
			if (ImGui::BeginTabItem("QoL")) {
				static bool noQuestBulbEnabled = true;
				static MemBytesPatch noQuestBulb(
					(void*)0x00A08D44,
					{ 0xeb, 0x10 }
				);

				if (ImGui::Checkbox("Quest Light Bulb", &noQuestBulbEnabled))
				{
					noQuestBulb.Toggle();
				}

				static bool noMapFadeEnabled = true;
				static MemBytesPatch NoMapFade(
					(void*)0x00776E65,
					{ 0xC3, 0x90, 0x90, 0x90, 0x90 }
				);

				if (ImGui::Checkbox("Map Fade", &noMapFadeEnabled))
				{
					NoMapFade.Toggle();
				}

				static bool aranComboEnabled = false;
				static ToggleGroup AranComboGroup{
					MemBytesPatch((void*)(0x00960581 + 3), { 0x3A, 0x07 }), // Aran Combo Numbers
					MemBytesPatch((void*)(0x00960839 + 1), { 0x3A, 0x07 }), // Aran Combo Letters Width
					MemBytesPatch((void*)(0x00960C67 + 1), { 0x70, 0x07 }), // Aran Combo SMASH / FENRIR Width
					MemBytesPatch((void*)(0x00960DED + 1), { 0x70, 0x07 })  // Aran Combo DRAIN Width
				};

				if (ImGui::Checkbox("Aran Combo for 1080p", &aranComboEnabled))
				{
					AranComboGroup.Toggle();
				}



				ImGui::EndTabItem();
			}

			// Colors Tab
			if (ImGui::BeginTabItem("Colors")) {
				// Dropdown for color settings
				if (ImGui::CollapsingHeader("Color Settings")) {
					auto DrawColorPicker = [](const char* label, uint32_t& color, uintptr_t address) {
						if (ImGui::Button(label)) {
							ImGui::OpenPopup(label);
						}
						if (ImGui::BeginPopup(label)) {
							if (RenderColorPicker(color)) {
								WriteValue(address + 1, color);
							}
							ImGui::EndPopup();
						}
						};

					if (ImGui::Button("Apply Color")) {
						SetDirectionMode(nullptr, 1);
						//SetDirectionMode(*INSTANCE_CUSERLOCAL, 0);
					}

					// Chat background color picker
					static uint32_t chatBgColor = 0x88000000;
					DrawColorPicker("Chat Background", chatBgColor, 0x008DBF61);

					// Tooltip color picker
					static uint32_t tooltipColor = 0xBB204491;
					if (ImGui::Button("Tooltip Color")) {
						ImGui::OpenPopup("Tooltip Color Picker");
					}
					if (ImGui::BeginPopup("Tooltip Color Picker")) {
						if (RenderColorPicker(tooltipColor)) {
							const uintptr_t tooltipAddresses[] = {
								0x008E6F35, 0x008E70C5, 0x008E7317, 0x008E7716, 0x008E7E49,
								0x008E97D2, 0x008EDBCF, 0x008EEEF1, 0x008F0460, 0x008F1D6B,
								0x008F214B, 0x008F22BB, 0x008F2876
							};
							for (uintptr_t addr : tooltipAddresses) {
								WriteValue(addr + 1, tooltipColor);
							}
						}
						ImGui::EndPopup();
					}

					// Chat Colors dropdown
					if (ImGui::CollapsingHeader("Chat Colors")) {
						static uint32_t noticeColor = 0xFF000000;
						DrawColorPicker("Game Notice", noticeColor, 0x008D0599);

						static uint32_t whisperColor = 0xFF000000;
						DrawColorPicker("Whisper", whisperColor, 0x008D0455);

						static uint32_t rewardColor = 0xFF000000;
						DrawColorPicker("Reward Message", rewardColor, 0x008D04F7);

						static uint32_t channelNoticeColor = 0xFF000000;
						DrawColorPicker("Channel Notice", channelNoticeColor, 0x008D0821);

						static uint32_t buddyColor = 0xFF000000;
						DrawColorPicker("Buddy", buddyColor, 0x008D0D2E);

						static uint32_t partyColor = 0xFF000000;
						DrawColorPicker("Party", partyColor, 0x008D0DD0);

						static uint32_t guildColor = 0xFF000000;
						DrawColorPicker("Guild", guildColor, 0x008D0FB0);

						static uint32_t allianceColor = 0xFF000000;
						DrawColorPicker("Alliance", allianceColor, 0x008D1052);

						static uint32_t worldChatColor = 0xFF000000;
						DrawColorPicker("World Chat", worldChatColor, 0x008D06DD);
					}
				}
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}
	}
	ImGui::End();
}







/**
	@brief : Hook of the EndScene function.
	@param  D3D9Device : Current Direct3D9 Device Object.
	@retval : Result of the original EndScene function.
**/
HRESULT Drawing::hkEndScene(const LPDIRECT3DDEVICE9 D3D9Device)
{
	if (!Hook::pDevice)
		Hook::pDevice = D3D9Device;

	if (!bInit)
		InitImGui(D3D9Device);

	if (GetAsyncKeyState(VK_INSERT) & 1)
		bDisplay = !bDisplay;
	
	if (GetAsyncKeyState(VK_END) & 1)
	{
		Hook::UnHookDirectX();
		CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)FreeLibrary, Hook::hDDLModule, 0, nullptr);
		return Hook::oEndScene(D3D9Device);
	}

	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();


	if (bDisplay)
	{
		draw();
	}

	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

	return Hook::oEndScene(D3D9Device);
}

/**
	@brief : function that init ImGui for rendering.
	@param pDevice : Current Direct3D9 Device Object given by the hooked function.
**/
void Drawing::InitImGui(const LPDIRECT3DDEVICE9 pDevice)
{
	D3DDEVICE_CREATION_PARAMETERS CP;
	pDevice->GetCreationParameters(&CP);
	Hook::window = CP.hFocusWindow;
	Hook::HookWindow();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.IniFilename = nullptr;
	io.Fonts->AddFontDefault();

	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(Hook::window);
	ImGui_ImplDX9_Init(pDevice);

	bInit = TRUE;
}