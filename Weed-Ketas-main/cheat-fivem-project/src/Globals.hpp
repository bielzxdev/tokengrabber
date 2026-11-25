#pragma once
#include <Includes/Includes.hpp>


inline std::atomic<DWORD> g_LastFiveMPid{ 0 };

class cColors {
public:
	// Accent: #4ADE80 (green), matching the web control panel.
	ImVec4 Base = ImColor( 74, 222, 128 );       //#4ADE80
	ImVec4 BaseLight = ImColor( 134, 239, 172 ); //#86EFAC
	ImVec4 BaseDark = ImColor( 20, 83, 45 );     //#14532D
	ImVec4 BaseFill = ImColor( 34, 197, 94 );    //#22C55E

	ImVec4 PrimaryText = ImColor( 244, 244, 245 );
	ImVec4 SecundaryText = ImColor( 113, 113, 122 );

	ImVec4 FeaturesText = ImColor( 212, 212, 216 );
	ImVec4 SecundaryFeaturesText = ImColor( 138, 138, 148 );
	ImVec4 DisabledText = ImColor( 82, 82, 91 );

	ImVec4 IconCol = ImColor( 190, 190, 198 );
	ImVec4 IconDisabledCol = ImColor( 110, 110, 120 );

	// Surfaces: the same near-black/green-neutral scale used by the website.
	ImVec4 BorderCol = ImColor( 31, 42, 35 );
	ImVec4 LinesCol = ImColor( 28, 39, 32 );
	ImVec4 BackgroundCol = ImColor( 8, 8, 9 );     //#080809

	ImVec4 ChildCol = ImColor( 13, 13, 15 );        //#0D0D0F
	ImVec4 ChildBorderCol = ImColor( 31, 43, 35 );  //#1F2B23

	ImVec4 TitleBar = ImColor( 10, 12, 11 );
	ImVec4 TitleBarBorder = ImColor( 28, 39, 32 );

	ImVec4 HeaderLeftCol = ImColor( 12, 45, 25 );  //#0C2D19
	ImVec4 HeaderRightCol = ImColor( 13, 13, 15 ); //#0D0D0F

	ImVec4 SideBar = ImColor( 8, 8, 9 );            //#080809
	ImVec4 SideBarTop = ImColor( 12, 25, 17 );
	ImVec4 SideBarBorder = ImColor( 29, 43, 33 );

	ImVec4 ActiveItemCol = ImColor( 13, 54, 29 );   //#0D361D
	ImVec4 ActiveBorderCol = ImColor( 74, 222, 128 ); //#4ADE80
	ImVec4 HoverCol = ImColor( 22, 31, 25 );        //#161F19

	ImVec4 BadgeCol = ImColor( 12, 44, 24 );        //#0C2C18
	ImVec4 BadgeBorderCol = ImColor( 32, 126, 69 ); //#207E45
	ImVec4 BadgeTextCol = ImColor( 134, 239, 172 ); //#86EFAC

	ImVec4 TrackCol = ImColor( 40, 48, 42 );        //#28302A

	ImVec4 HealthFull = ImColor( 74, 222, 128 );    //#4ADE80

	// Toggle ligado: verde do painel, com o botao claro por cima.
	ImVec4 ToggleOn = ImColor( 34, 197, 94 );       //#22C55E
	ImVec4 ToggleOnHover = ImColor( 74, 222, 128 ); //#4ADE80
	ImVec4 ToggleMark = ImColor( 3, 20, 10 );       //#03140A

	ImVec4 ButtonHovered = ImColor( 34, 197, 94, 220 );

	ImVec4 InputBackground = ImColor( 18, 24, 20, 255 );
	ImVec4 InputBorder = ImColor( 34, 55, 40, 255 );

	// Notification surface / accent icon
	ImVec4 NotifyBackground = ImColor( 13, 13, 15 ); //#0D0D0F
	ImVec4 NotifyBorder = ImColor( 31, 43, 35 );
	ImVec4 NotifyIconCol = ImColor( 234, 179, 8 );   //#EAB308
	ImVec4 NotifyIconGlyph = ImColor( 32, 26, 6 );
};

inline cColors g_Col;

class c_globals {
public:
	std::string id;
	std::string version;
	std::string UserName;
	std::string Role;

	bool done;
	bool IsOpen;
	std::string ServerIp;

	DWORD ProcIdFiveM = 0;

	HWND g_hCheatWindow;
	HWND g_hGameWindow;
	ImVec2 g_vGameWindowSize;
	ImVec2 g_vGameWindowPos;
	ImVec2 g_vGameWindowCenter;

	ImFont * m_FontBig;
	ImFont * m_FontBigSmall;
	ImFont * m_FontNormal;
	ImFont * m_FontSecundary;
	ImFont * m_FontSmaller;
	ImFont * m_DrawFont;
	// Fontes do ESP escolhiveis no menu. Nao da para identificar tipografia
	// a partir de screenshot comprimido, entao a escolha fica com o usuario.
	static constexpr int kEspFontCount = 9;
	ImFont * m_EspFonts[ kEspFontCount ]{};
	ImFont * m_FontEsp = nullptr;   // aponta para a selecionada
	ImFont * m_Expand;

	ImFont * FontAwesomeSolid;
	ImFont * FontAwesomeSolidSmall;
	ImFont * FontAwesomeRegular;
	ImFont * FontAwesomeBrands;


	GLTexture Logo;
	struct FiveM_Weapons_t {
		GLTexture gadget_parachute;
		GLTexture weapon_advancedrifle;
		GLTexture weapon_appistol;
		GLTexture weapon_assaultrifle;
		GLTexture weapon_assaultrifle_mk2;
		GLTexture weapon_assaultshotgun;
		GLTexture weapon_assaultsmg;
		GLTexture weapon_autoshotgun;
		GLTexture weapon_ball;
		GLTexture weapon_bat;
		GLTexture weapon_battleaxe;
		GLTexture weapon_bottle;
		GLTexture weapon_bullpuprifle;
		GLTexture weapon_bullpuprifle_mk2;
		GLTexture weapon_bullpupshotgun;
		GLTexture weapon_bzgas;
		GLTexture weapon_carbinerifle;
		GLTexture weapon_carbinerifle_mk2;
		GLTexture weapon_ceramicpistol;
		GLTexture weapon_combatmg;
		GLTexture weapon_combatmg_mk2;
		GLTexture weapon_combatpdw;
		GLTexture weapon_combatpistol;
		GLTexture weapon_combatshotgun;
		GLTexture weapon_compactlauncher;
		GLTexture weapon_compactrifle;
		GLTexture weapon_crowbar;
		GLTexture weapon_dagger;
		GLTexture weapon_dbshotgun;
		GLTexture weapon_doubleaction;
		GLTexture weapon_fireextinguisher;
		GLTexture weapon_firework;
		GLTexture weapon_flare;
		GLTexture weapon_flaregun;
		GLTexture weapon_flashlight;
		GLTexture weapon_gadgetpistol;
		GLTexture weapon_golfclub;
		GLTexture weapon_grenade;
		GLTexture weapon_grenadelauncher;
		GLTexture weapon_grenadelauncher_smoke;
		GLTexture weapon_gusenberg;
		GLTexture weapon_hammer;
		GLTexture weapon_hatchet;
		GLTexture weapon_hazardcan;
		GLTexture weapon_heavypistol;
		GLTexture weapon_heavyshotgun;
		GLTexture weapon_heavysniper;
		GLTexture weapon_heavysniper_mk2;
		GLTexture weapon_hominglauncher;
		GLTexture weapon_knife;
		GLTexture weapon_knuckle;
		GLTexture weapon_machete;
		GLTexture weapon_machinepistol;
		GLTexture weapon_marksmanpistol;
		GLTexture weapon_marksmanrifle;
		GLTexture weapon_marksmanrifle_mk2;
		GLTexture weapon_mg;
		GLTexture weapon_microsmg;
		GLTexture weapon_militaryrifle;
		GLTexture weapon_minigun;
		GLTexture weapon_minismg;
		GLTexture weapon_molotov;
		GLTexture weapon_musket;
		GLTexture weapon_navyrevolver;
		GLTexture weapon_nightstick;
		GLTexture weapon_petrolcan;
		GLTexture weapon_pipebomb;
		GLTexture weapon_pistol;
		GLTexture weapon_pistol50;
		GLTexture weapon_pistol_mk2;
		GLTexture weapon_poolcue;
		GLTexture weapon_proxmine;
		GLTexture weapon_pumpshotgun;
		GLTexture weapon_pumpshotgun_mk2;
		GLTexture weapon_railgun;
		GLTexture weapon_raycarbine;
		GLTexture weapon_rayminigun;
		GLTexture weapon_raypistol;
		GLTexture weapon_revolver;
		GLTexture weapon_revolver_mk2;
		GLTexture weapon_rpg;
		GLTexture weapon_sawnoffshotgun;
		GLTexture weapon_smg;
		GLTexture weapon_smg_mk2;
		GLTexture weapon_smokegrenade;
		GLTexture weapon_sniperrifle;
		GLTexture weapon_snowball;
		GLTexture weapon_snspistol;
		GLTexture weapon_snspistol_mk2;
		GLTexture weapon_specialcarbine;
		GLTexture weapon_specialcarbine_mk2;
		GLTexture weapon_stickybomb;
		GLTexture weapon_stone_hatchet;
		GLTexture weapon_stungun;
		GLTexture weapon_switchblade;
		GLTexture weapon_unarmed;
		GLTexture weapon_vintagepistol;
		GLTexture weapon_wrench;
	}FiveM_WeaponsPic;
	
};

inline c_globals g_Variables;

class c_menu {
public:

	enum PAGES {
		Combat,
		Visuals,
		Player,
		Local,
		World,
		Exploits,
		Settings,
		Login
	};

	bool IsOpen;

	int iTabCount = 0;
	float TabAlpha = 0.f;
	int iCurrentPage = 6;
	float TabAdd = 0.f;
	bool IsLogged = false;

	ImVec2 MenuSize = { 700, 400 };
	ImVec2 TabsTopBackgroundPos;

	// Layout metrics: todo container deriva do canvas 700x400.
	//
	// Largura: CardX(200 = SideBar 192 + 8 de folga) + 2 cards + gap.
	//   200 + 242 + 7 + 242 = 691, sobrando 9 de margem direita.
	// Altura: CardY(36) + CardHeight(356) = 392, sobrando 8 embaixo.
	// Mexer em MenuSize pede refazer CardWidth/CardHeight por essas contas,
	// senao os cards encostam na borda ou sobra um vao no meio.
	float WindowRounding = 5.f;

	// Barra de titulo no topo, atravessando a janela inteira. Tudo abaixo dela
	// e deslocado por esta altura -- se mexer aqui, NavStartY e CardY seguem.
	float TitleBarHeight = 28.f;

	float SideBarWidth = 192.f;
	float SideBarLogoY = 7.f;   // (nao usado: a marca foi para a barra de titulo)
	float SideBarLogoSize = 22.f;
	float SideBarSeparatorY = 34.f; // (nao usado)
	float SideBarPadding = 10.f;

	// Profile block is docked to the bottom-left: ProfileY is its distance from the window bottom.
	float ProfileY = 8.f;
	float ProfileHeight = 46.f;
	float AvatarSize = 26.f;

	float NavStartY = 40.f;   // 28 da barra de titulo + 12 de folga
	float TabWidth = 172.f;
	float TabHeight = 28.f;
	float TabSpacing = 1.f;
	float SubTabHeight = 21.f;
	float SubTabIndent = 26.f;

	float CardX = 200.f;
	float CardY = 36.f;
	float CardWidth = 242.f;
	float CardHeight = 356.f;
	float CardGap = 7.f;
	float CardHeaderHeight = 32.f;
	float CardRounding = 4.f;
	float CardPadding = 12.f;

	// Control metrics: every row shares the same height so labels/controls line up on one grid.
	float RowHeight = 21.f;
	float RowSpacing = 5.f;
	// Every boolean row renders as the animated switch (Toggle / CheckBox / CheckBoxCfg / CheckBoxPage)
	// A caixa do checkbox e QUADRADA. ToggleWidth continua existindo porque a
	// amostra de cor (ColorRow) usa o retangulo largo.
	float CheckBoxSize = 17.f;
	float ToggleWidth = 30.f;
	float ToggleHeight = 16.f;
	float ControlRounding = 3.f;
	float ButtonHeight = 26.f;
	float ListRowHeight = 24.f;
	float SliderHeight = 14.f;
	float SliderTrack = 7.f;

	// Page/subtab transition timing (units per second)
	float TransitionOut = 12.f;
	float TransitionIn = 7.f;
	float TransitionSlide = 12.f;
};

inline c_menu g_MenuInfo;
