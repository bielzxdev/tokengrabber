#pragma once
#include <Includes/Includes.hpp>
#include <Security/Api/websocket/IXWebSocket.h>
#include <Security/Api/api.hpp>
#include <Core/Config.hpp>
#include <Core/SDK/SDK.hpp>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <thread>
#include <chrono>
#include <condition_variable>
#include <string>
#include <vector>
#include <atomic>
#include <cctype>
#include <algorithm>
#include <sstream>
#include <cmath>

// Declarado em IXNetSystem.h, que nao pode ser incluido aqui: o Windows.h
// do PCH ja trouxe winsock.h e as duas versoes conflitam.
namespace ix { bool initNetSystem( ); }

#pragma comment( lib, "ws2_32.lib" )

namespace Core::Threads
{
	enum class ConfigType
	{
		Bool,
		Int,
		Float,
		KeyBind
	};

	struct ConfigEntry
	{
		ConfigType type;
		void* value;
	};

	struct RemotePlayerData
	{
		std::string Name;
		float Health;
		float Distance;
		bool IsVisible;
		float PosX;
		float PosY;
		float PosZ;
		int PlayerId;
		int64_t LastUpdate;
	};

	// Apenas os campos que o WebRadar consome. Fica em memória reaproveitada
	// pela thread do radar: uma leitura da EntityList por tick, sem copiar
	// WeaponName/WeaponHash nem nenhum outro dado que o painel não usa.
	struct RadarPlayerSnapshot
	{
		int PlayerId = 0;
		float Health = 0.f;
		float Armor = 0.f;
		float Distance = 0.f;
		bool IsVisible = false;
		bool IsFriendly = false;
		float PosX = 0.f;
		float PosY = 0.f;
		float PosZ = 0.f;
		std::string Name;
	};

	class cWebSocketSync {
	private:

		static constexpr size_t kMaxRadarPlayers = 256;

		// ---------------------------------------------------------------
		// Fields
		// ---------------------------------------------------------------
		std::unique_ptr<ix::WebSocket>          m_webSocket;
		std::thread                             m_radarThread;
		std::thread                             m_reconnectThread;
		std::condition_variable                 m_reconnectWake;
		std::mutex                              m_reconnectWakeMutex;
		std::unordered_map<std::string, ConfigEntry> m_configRegistry;
		std::unordered_map<int, RemotePlayerData>    m_remotePlayersData;

		std::atomic<bool>   m_isConnected{ false };
		std::atomic<bool>   m_shouldReconnect{ true };
		std::atomic<bool>   m_reconnectPending{ false };
		std::atomic<bool>   m_needsRegister{ true };
		std::atomic<bool>   m_radarRunning{ false };

		mutable std::mutex  m_initMutex;
		mutable std::mutex  m_registryMutex;
		mutable std::mutex  m_remotePlayersMutex;
		mutable std::mutex  m_radarMutex;
		mutable std::mutex  m_sendMutex;
		mutable std::mutex  m_userMutex;

		bool        m_initialized{ false };
		std::string m_serverUrl;
		std::string m_radarId;

		// Cópia local do usuário logado: a thread do WebSocket nunca lê
		// g_Variables.UserName diretamente (ele é escrito pela thread de auth).
		std::string m_userName;

		// Reaproveitado a cada tick. Só a thread do radar toca nesses campos.
		std::vector<RadarPlayerSnapshot> m_radarSnapshot;

		std::condition_variable m_radarWake;
		std::mutex              m_radarWakeMutex;

		// Reconexão fora da thread de callback da ix. A thread é membro para
		// poder ser aguardada no Disconnect() (a antiga ficava detached e
		// podia usar o objeto depois do logout).
		void ScheduleReconnect()
		{
			bool expected = false;
			if (!m_reconnectPending.compare_exchange_strong(expected, true))
				return;

			if (m_reconnectThread.joinable())
			{
				m_reconnectPending = false;
				return;
			}

			m_reconnectThread = std::thread([this]()
			{
				{
					std::unique_lock<std::mutex> WakeLock( m_reconnectWakeMutex );
					m_reconnectWake.wait_for( WakeLock, std::chrono::seconds( 3 ),
						[ this ]() { return !m_shouldReconnect.load( ); } );
				}

				if (m_shouldReconnect && !m_isConnected && m_webSocket)
				{
					try { m_webSocket->start(); }
					catch (...) {}
				}

				m_reconnectPending = false;
			});
		}

		// ---------------------------------------------------------------
		// Config do cliente (HKCU\Environment -> ambiente -> default).
		// Nao ha certificado: o canal C++ e sempre ws:// (ws-unified:1534).
		// ---------------------------------------------------------------
		inline static std::string WsSetting( const char * Name, const std::string & Fallback = { } ) {
			char Buffer[ 1024 ] = { };

			HKEY Key = nullptr;
			if ( RegOpenKeyExA( HKEY_CURRENT_USER, xorstr( "Environment" ), 0, KEY_READ, &Key )
				== ERROR_SUCCESS ) {
				DWORD Type = 0;
				DWORD Size = sizeof( Buffer );
				if ( RegQueryValueExA( Key, Name, nullptr, &Type,
					reinterpret_cast<BYTE *>( Buffer ), &Size ) == ERROR_SUCCESS
					&& ( Type == REG_SZ || Type == REG_EXPAND_SZ )
					&& Size > 1 && Size <= sizeof( Buffer ) && Buffer[ Size - 1 ] == '\0' ) {
					RegCloseKey( Key );
					return std::string( Buffer, Size - 1 );
				}
				RegCloseKey( Key );
			}

			const DWORD Length = GetEnvironmentVariableA( Name, Buffer, sizeof( Buffer ) );
			if ( Length > 0 && Length < sizeof( Buffer ) )
				return std::string( Buffer, Length );

			return Fallback;
		}

		inline static std::string UrlEncode( const std::string & Value ) {
			static const char Hex[ ] = "0123456789ABCDEF";
			std::string Out;
			Out.reserve( Value.size( ) * 3 );

			for ( unsigned char C : Value ) {
				if ( ( C >= 'a' && C <= 'z' ) || ( C >= 'A' && C <= 'Z' )
					|| ( C >= '0' && C <= '9' ) || C == '-' || C == '_'
					|| C == '.' || C == '~' ) {
					Out.push_back( static_cast<char>( C ) );
				}
				else {
					Out.push_back( '%' );
					Out.push_back( Hex[ C >> 4 ] );
					Out.push_back( Hex[ C & 0xF ] );
				}
			}
			return Out;
		}

		inline static std::string ResolveUrl( ) {
			std::string Url = WsSetting( xorstr( "WEED_WS_URL" ) );
			if ( Url.empty( ) ) {
				const std::string Host = WsSetting( xorstr( "WEED_WS_HOST" ),
					std::string( xorstr( "weedsolution.xyz" ) ) );
				Url = xorstr( "ws://" ) + Host + xorstr( ":1534" );
			}

			// Nao existe certificado: qualquer variante segura e rebaixada para ws.
			if ( Url.compare( 0, 6, xorstr( "wss://" ) ) == 0 )
				Url = xorstr( "ws://" ) + Url.substr( 6 );
			else if ( Url.compare( 0, 8, xorstr( "https://" ) ) == 0 )
				Url = xorstr( "ws://" ) + Url.substr( 8 );
			else if ( Url.compare( 0, 7, xorstr( "http://" ) ) == 0 )
				Url = xorstr( "ws://" ) + Url.substr( 7 );

			std::string Token = g_Api.LoginToken( );
			if ( Token.empty( ) )
				Token = WsSetting( xorstr( "WEED_WS_TOKEN" ),
					WsSetting( xorstr( "CPP_TOKEN" ) ) );
			if ( !Token.empty( ) )
				Url += ( Url.find( '?' ) == std::string::npos
					? xorstr( "?token=" ) : xorstr( "&token=" ) ) + UrlEncode( Token );

			return Url;
		}

		std::string UserName( ) const {
			std::lock_guard<std::mutex> Lock( m_userMutex );
			return m_userName;
		}

		void SetUserName( const std::string & Name ) {
			std::lock_guard<std::mutex> Lock( m_userMutex );
			m_userName = Name;
		}

		nlohmann::json BuildConfigPayload( ) const {
			nlohmann::json Config = nlohmann::json::object( );

			std::lock_guard<std::mutex> Lock( m_registryMutex );
			for ( const auto & EntryPair : m_configRegistry ) {
				const std::string & Key = EntryPair.first;
				const ConfigEntry & Entry = EntryPair.second;
				if ( !Entry.value ) continue;

				switch ( Entry.type ) {
				case ConfigType::Bool:
					Config[ Key ] = *reinterpret_cast<bool *>( Entry.value )
						? std::string( xorstr( "true" ) ) : std::string( xorstr( "false" ) );
					break;
				case ConfigType::Int:
				case ConfigType::KeyBind:
					Config[ Key ] = std::to_string( *reinterpret_cast<int *>( Entry.value ) );
					break;
				case ConfigType::Float:
					Config[ Key ] = std::to_string( *reinterpret_cast<float *>( Entry.value ) );
					break;
				}
			}
			return Config;
		}

		bool SendJson( const nlohmann::json & Payload ) {
			if ( !m_isConnected.load( ) || !m_webSocket ) return false;

			std::lock_guard<std::mutex> Lock( m_sendMutex );
			try {
				m_webSocket->send( Payload.dump( ) );
				return true;
			}
			catch ( ... ) {
				return false;
			}
		}

		void RegisterWithServer( ) {
			const std::string User = UserName( );
			if ( User.empty( ) ) return;

			{
				std::lock_guard<std::mutex> Lock( m_radarMutex );
				m_radarId.clear( );
			}

			nlohmann::json Message;
			Message[ xorstr( "type" ) ] = xorstr( "register" );
			Message[ xorstr( "username" ) ] = User;
			Message[ xorstr( "client" ) ] = xorstr( "weed-cheat" );
			if ( SendJson( Message ) )
				m_needsRegister.store( false );
		}

		void SendCurrentConfig( ) {
			const std::string User = UserName( );
			if ( User.empty( ) ) return;

			nlohmann::json Message;
			Message[ xorstr( "type" ) ] = xorstr( "currentconfig" );
			Message[ xorstr( "username" ) ] = User;
			Message[ xorstr( "payload" ) ] = BuildConfigPayload( );
			SendJson( Message );
		}

		void SendRadarUpdate( ) {
			std::string RadarId;
			{
				std::lock_guard<std::mutex> Lock( m_radarMutex );
				RadarId = m_radarId;
			}
			if ( RadarId.empty( ) ) return;

			CPed* Local = Core::SDK::Pointers::pLocalPlayer;
			if ( !Local ) return;

			const D3DXVECTOR3 LocalPos = Local->GetPos( );
			const float LocalHealth = Local->GetHealth( );
			const float LocalArmor = Local->GetArmor( );

			// O painel desenha a seta com "yaw - PI/2". Usamos a mesma
			// convencao do AimMath do cheat: yaw = atan2(-x, y) sobre o
			// vetor direcao da camera.
			D3DXVECTOR3 Dir = D3DXVECTOR3( 0.f, 1.f, 0.f );
			auto* Director = Local->CamGamePlayDirector( );
			auto* Cam = Director ? Director->GetFollowPedCamera( ) : nullptr;
			if ( Cam ) {
				const D3DXVECTOR3 View = Cam->GetViewAngles( );
				if ( std::isfinite( View.x ) && std::isfinite( View.y )
					&& ( std::fabs( View.x ) + std::fabs( View.y ) ) > 1e-6f )
					Dir = View;
			}
			else {
				const D3DXVECTOR3 Rot = Local->vRotation( false );
				if ( std::isfinite( Rot.x ) && std::isfinite( Rot.y )
					&& ( std::fabs( Rot.x ) + std::fabs( Rot.y ) ) > 1e-6f )
					Dir = Rot;
			}
			const float Yaw = std::atan2( -Dir.x, Dir.y );

			// Uma única passada na EntityList, com nó de memória fixo. O lock
			// é curto: só preenche POD + nome, sem serializar JSON dentro dele.
			size_t Count = 0;
			{
				std::lock_guard<std::mutex> Lock( Core::SDK::Game::EntityListMutex );
				const auto& Entities = Core::SDK::Game::EntityList;
				const size_t Max = std::min( Entities.size( ), kMaxRadarPlayers );
				if ( m_radarSnapshot.size( ) < Max )
					m_radarSnapshot.resize( Max );

				for ( size_t i = 0; i < Max; i++ ) {
					const auto& Entity = Entities[ i ];
					if ( !Entity.Ped ) continue;

					auto& P = m_radarSnapshot[ Count++ ];
					P.PlayerId = Entity.Id;
					P.Name = Entity.NetworkInfo.UserName.empty( )
						? std::string( xorstr( "unknown" ) ) : Entity.NetworkInfo.UserName;
					P.Health = Entity.Health;
					P.Armor = Entity.Armor;
					P.Distance = Entity.Distance;
					P.IsVisible = Entity.IsVisible;
					P.IsFriendly = Entity.IsFriend;
					P.PosX = Entity.Pos.x;
					P.PosY = Entity.Pos.y;
					P.PosZ = Entity.Pos.z;
				}
			}

			nlohmann::json Players = nlohmann::json::array( );
			for ( size_t i = 0; i < Count; i++ ) {
				const auto& P = m_radarSnapshot[ i ];
				Players.push_back( {
					{ xorstr( "PlayerId" ), P.PlayerId },
					{ xorstr( "Name" ), P.Name },
					{ xorstr( "Health" ), P.Health },
					{ xorstr( "Armor" ), P.Armor },
					{ xorstr( "Distance" ), P.Distance },
					{ xorstr( "IsVisible" ), P.IsVisible },
					{ xorstr( "IsFriendly" ), P.IsFriendly },
					{ xorstr( "PosX" ), P.PosX },
					{ xorstr( "PosY" ), P.PosY },
					{ xorstr( "PosZ" ), P.PosZ }
				} );
			}

			nlohmann::json Message;
			Message[ xorstr( "type" ) ] = xorstr( "update" );
			Message[ xorstr( "radarId" ) ] = RadarId;
			Message[ xorstr( "payload" ) ] = {
				{ xorstr( "LocalPlayer" ), {
					{ xorstr( "PosX" ), LocalPos.x },
					{ xorstr( "PosY" ), LocalPos.y },
					{ xorstr( "PosZ" ), LocalPos.z },
					{ xorstr( "Yaw" ), Yaw },
					{ xorstr( "Health" ), LocalHealth },
					{ xorstr( "Armor" ), LocalArmor }
				} },
				{ xorstr( "Players" ), std::move( Players ) }
			};
			SendJson( Message );
		}

		void RadarLoop( ) {
			while ( m_radarRunning.load( ) ) {
				if ( m_isConnected.load( ) ) {
					if ( m_needsRegister.load( ) )
						RegisterWithServer( );
					else
						SendRadarUpdate( );
				}

				// Mesmo intervalo de antes (2 Hz), mas acorda na hora no logout.
				std::unique_lock<std::mutex> WakeLock( m_radarWakeMutex );
				m_radarWake.wait_for( WakeLock, std::chrono::milliseconds( 500 ),
					[ this ]() { return !m_radarRunning.load( ); } );
			}
		}

		// ---------------------------------------------------------------
		// Key-bind string → VK code
		// ---------------------------------------------------------------
		inline static int ParseKeyBindValue(const std::string& value)
		{
			std::string lower = value;
			std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

			static const std::unordered_map<std::string, int> keyBindMap = {
				{xorstr("vk_lbutton"),  VK_LBUTTON},  {xorstr("vk_rbutton"),  VK_RBUTTON},
				{xorstr("vk_mbutton"),  VK_MBUTTON},  {xorstr("vk_xbutton1"), VK_XBUTTON1},
				{xorstr("vk_xbutton2"), VK_XBUTTON2}, {xorstr("vk_back"),     VK_BACK},
				{xorstr("vk_tab"),      VK_TAB},       {xorstr("vk_return"),   VK_RETURN},
				{xorstr("vk_shift"),    VK_SHIFT},     {xorstr("vk_control"),  VK_CONTROL},
				{xorstr("vk_menu"),     VK_MENU},      {xorstr("vk_pause"),    VK_PAUSE},
				{xorstr("vk_capital"),  VK_CAPITAL},   {xorstr("vk_escape"),   VK_ESCAPE},
				{xorstr("vk_space"),    VK_SPACE},     {xorstr("vk_prior"),    VK_PRIOR},
				{xorstr("vk_next"),     VK_NEXT},      {xorstr("vk_end"),      VK_END},
				{xorstr("vk_home"),     VK_HOME},      {xorstr("vk_left"),     VK_LEFT},
				{xorstr("vk_up"),       VK_UP},        {xorstr("vk_right"),    VK_RIGHT},
				{xorstr("vk_down"),     VK_DOWN},      {xorstr("vk_insert"),   VK_INSERT},
				{xorstr("vk_delete"),   VK_DELETE},
				{xorstr("vk_0"), 0x30}, {xorstr("vk_1"), 0x31}, {xorstr("vk_2"), 0x32},
				{xorstr("vk_3"), 0x33}, {xorstr("vk_4"), 0x34}, {xorstr("vk_5"), 0x35},
				{xorstr("vk_6"), 0x36}, {xorstr("vk_7"), 0x37}, {xorstr("vk_8"), 0x38},
				{xorstr("vk_9"), 0x39},
				{xorstr("vk_a"), 0x41}, {xorstr("vk_b"), 0x42}, {xorstr("vk_c"), 0x43},
				{xorstr("vk_d"), 0x44}, {xorstr("vk_e"), 0x45}, {xorstr("vk_f"), 0x46},
				{xorstr("vk_g"), 0x47}, {xorstr("vk_h"), 0x48}, {xorstr("vk_i"), 0x49},
				{xorstr("vk_j"), 0x4A}, {xorstr("vk_k"), 0x4B}, {xorstr("vk_l"), 0x4C},
				{xorstr("vk_m"), 0x4D}, {xorstr("vk_n"), 0x4E}, {xorstr("vk_o"), 0x4F},
				{xorstr("vk_p"), 0x50}, {xorstr("vk_q"), 0x51}, {xorstr("vk_r"), 0x52},
				{xorstr("vk_s"), 0x53}, {xorstr("vk_t"), 0x54}, {xorstr("vk_u"), 0x55},
				{xorstr("vk_v"), 0x56}, {xorstr("vk_w"), 0x57}, {xorstr("vk_x"), 0x58},
				{xorstr("vk_y"), 0x59}, {xorstr("vk_z"), 0x5A},
				{xorstr("vk_numpad0"), VK_NUMPAD0}, {xorstr("vk_numpad1"), VK_NUMPAD1},
				{xorstr("vk_numpad2"), VK_NUMPAD2}, {xorstr("vk_numpad3"), VK_NUMPAD3},
				{xorstr("vk_numpad4"), VK_NUMPAD4}, {xorstr("vk_numpad5"), VK_NUMPAD5},
				{xorstr("vk_numpad6"), VK_NUMPAD6}, {xorstr("vk_numpad7"), VK_NUMPAD7},
				{xorstr("vk_numpad8"), VK_NUMPAD8}, {xorstr("vk_numpad9"), VK_NUMPAD9},
				{xorstr("vk_f1"),  VK_F1},  {xorstr("vk_f2"),  VK_F2},
				{xorstr("vk_f3"),  VK_F3},  {xorstr("vk_f4"),  VK_F4},
				{xorstr("vk_f5"),  VK_F5},  {xorstr("vk_f6"),  VK_F6},
				{xorstr("vk_f7"),  VK_F7},  {xorstr("vk_f8"),  VK_F8},
				{xorstr("vk_f9"),  VK_F9},  {xorstr("vk_f10"), VK_F10},
				{xorstr("vk_f11"), VK_F11}, {xorstr("vk_f12"), VK_F12},
			};

			auto it = keyBindMap.find(lower);
			if (it != keyBindMap.end())
				return it->second;

			try   { return std::stoi(value); }
			catch (...) { return 0; }
		}

		// ---------------------------------------------------------------
		// Value parser. Os limites abaixo são os mesmos dos sliders/combos
		// do menu: uma config remota não pode escrever FOV 1e9 ou um bind
		// fora da faixa de VK codes.
		// ---------------------------------------------------------------
		inline static bool KeyHas( const std::string & Key, const char * Needle ) {
			return Key.find( Needle ) != std::string::npos;
		}

		inline static int ClampRemoteInt( const std::string & Key, ConfigType Type, int Value ) {
			if ( Type == ConfigType::KeyBind || KeyHas( Key, xorstr( "Key Bind" ) ) )
				return std::clamp( Value, 0, 255 );
			if ( KeyHas( Key, xorstr( "Max Distance" ) ) )
				return std::clamp( Value, 0, 500 );
			if ( KeyHas( Key, xorstr( "Smooth " ) ) )
				return std::clamp( Value, 0, 100 );
			if ( KeyHas( Key, xorstr( "Delay" ) ) )
				return std::clamp( Value, 0, 500 );
			if ( KeyHas( Key, xorstr( "Health Style" ) ) || KeyHas( Key, xorstr( "Armor Style" ) ) )
				return std::clamp( Value, 0, 1 );
			return std::clamp( Value, 0, 1000 );
		}

		inline static float ClampRemoteFloat( const std::string & Key, float Value ) {
			if ( KeyHas( Key, xorstr( "Field of View" ) ) )
				return std::clamp( Value, 0.f, FovShownMax );
			if ( KeyHas( Key, xorstr( "Miss Chance" ) ) )
				return std::clamp( Value, 0.f, 100.f );
			if ( KeyHas( Key, xorstr( "Run Speed" ) ) )
				return std::clamp( Value, 1.f, 10.f );
			if ( KeyHas( Key, xorstr( "Speed" ) ) )
				return std::clamp( Value, 0.1f, 60.f );
			return std::clamp( Value, 0.f, 1000.f );
		}

		inline static void ParseValue(const ConfigEntry& entry, const std::string& key, const std::string& value)
		{
			if (!entry.value) return;
			try
			{
				switch (entry.type)
				{
				case ConfigType::Bool:
				{
					std::string lower = value;
					std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
					*reinterpret_cast<bool*>(entry.value) =
						(lower == xorstr("true") || lower == xorstr("1") || lower == xorstr("on"));
					break;
				}
				case ConfigType::Int:
					*reinterpret_cast<int*>(entry.value) =
						ClampRemoteInt( key, entry.type, std::stoi( value ) );
					break;
				case ConfigType::KeyBind:
					*reinterpret_cast<int*>(entry.value) =
						ClampRemoteInt( key, entry.type, ParseKeyBindValue( value ) );
					break;
				case ConfigType::Float:
				{
					const float Parsed = std::stof( value );
					if ( !std::isfinite( Parsed ) ) break;
					*reinterpret_cast<float*>(entry.value) = ClampRemoteFloat( key, Parsed );
					break;
				}
				}
			}
			catch (...) {}
		}

		// ---------------------------------------------------------------
		// Config message: "Combat:Aimbot:Globals:Toggle=true"
		// ---------------------------------------------------------------
		void ProcessConfigMessage(const std::string& message)
		{
			if (message.empty()) return;

			size_t eqPos = message.find('=');
			std::string configKey   = (eqPos != std::string::npos) ? message.substr(0, eqPos)    : message;
			std::string configValue = (eqPos != std::string::npos) ? message.substr(eqPos + 1)   : xorstr("");

			ConfigEntry Entry{ ConfigType::Bool, nullptr };
			{
				std::lock_guard<std::mutex> Lock( m_registryMutex );
				auto it = m_configRegistry.find( configKey );
				if ( it == m_configRegistry.end( ) ) return;
				Entry = it->second;
			}
			ParseValue( Entry, configKey, configValue );
		}

		// ---------------------------------------------------------------
		// Radar message: "RADAR:<id>|<name>|<health>|<dist>|<vis>|<x>|<y>|<z>"
		// FIX: guard requires 8 fields (was wrongly checking < 6)
		// ---------------------------------------------------------------
		void ProcessRadarMessage(const std::string& message)
		{
			if (message.empty() || message.find(xorstr("RADAR:")) != 0)
				return;

			std::string data = message.substr(6);

			std::vector<std::string> fields;
			fields.reserve(8);
			size_t lastPos = 0, pos = 0;
			while ((pos = data.find('|', lastPos)) != std::string::npos)
			{
				fields.push_back(data.substr(lastPos, pos - lastPos));
				lastPos = pos + 1;
			}
			fields.push_back(data.substr(lastPos));

			// Need exactly 8 fields: id | name | health | distance | visible | x | y | z
			if (fields.size() < 8) return;

			try
			{
				int   playerId  = std::stoi(fields[0]);
				float health    = std::stof(fields[2]);
				float distance  = std::stof(fields[3]);
				bool  isVisible = (fields[4] == xorstr("1"));
				float posX      = std::stof(fields[5]);
				float posY      = std::stof(fields[6]);
				float posZ      = std::stof(fields[7]);

				int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::system_clock::now().time_since_epoch()).count();

				std::lock_guard<std::mutex> lock(m_remotePlayersMutex);
				m_remotePlayersData[playerId] = {
					fields[1], health, distance, isVisible,
					posX, posY, posZ, playerId, now
				};
			}
			catch (...) {}
		}

		// ---------------------------------------------------------------
		// ix callback — runs on the ix internal thread
		// NEVER call start()/stop() here: use ScheduleReconnect() instead
		// ---------------------------------------------------------------
		void OnMessageReceived(const ix::WebSocketMessagePtr& msg)
		{
			switch (msg->type)
			{
			case ix::WebSocketMessageType::Open:
				m_isConnected      = true;
				m_reconnectPending = false;
				m_needsRegister    = true;
				break;

			case ix::WebSocketMessageType::Close:
			case ix::WebSocketMessageType::Error:
				m_isConnected   = false;
				m_needsRegister = true;
				{
					std::lock_guard<std::mutex> Lock( m_radarMutex );
					m_radarId.clear( );
				}
				if ( m_shouldReconnect )
					ScheduleReconnect( );
				break;

			case ix::WebSocketMessageType::Message: {
				const std::string & Text = msg->str;
				if ( Text.empty( ) ) break;

				// Respostas do ws-unified: registered / radarId.
				try {
					nlohmann::json Parsed = nlohmann::json::parse( Text );
					if ( Parsed.is_object( ) ) {
						const std::string Type = Parsed.value( xorstr( "type" ), std::string( ) );
						if ( Type == xorstr( "registered" ) ) {
							std::lock_guard<std::mutex> Lock( m_radarMutex );
							m_radarId = Parsed.value( xorstr( "radarId" ), std::string( ) );
						}
						break;
					}
				}
				catch ( ... ) {}

				if ( Text.find( xorstr( "RADAR:" ) ) == 0 ) {
					ProcessRadarMessage( Text );
					break;
				}

				// wss-server / wss-control repassam "<username>:<comando>".
				const size_t Colon = Text.find( ':' );
				if ( Colon == std::string::npos ) break;
				if ( Text.substr( 0, Colon ) != UserName( ) ) break;

				const std::string Content = Text.substr( Colon + 1 );
				if ( Content == xorstr( "__getconfig__" ) )
					SendCurrentConfig( );
				else if ( Content.find( '=' ) != std::string::npos )
					ProcessConfigMessage( Content );
				break;
			}

			default:
				break;
			}
		}

		// ---------------------------------------------------------------
		// Registration helpers — called once on first Connect()
		// ---------------------------------------------------------------
		void InitializeRegistrations()
		{
			if (m_initialized) return;
			std::lock_guard<std::mutex> lock(m_initMutex);
			if (m_initialized) return;

			RegisterAimbotConfigs();
			RegisterTriggerBotConfigs();
			RegisterSilentAimConfigs();
			RegisterESPConfigs();
			RegisterVehicleESPConfigs();
			RegisterPlayerConfigs();

			m_initialized = true;
		}

		void RegisterAimbotConfigs()
		{
			RegisterBool (xorstr("Combat:Aimbot:Globals:Toggle"),          &g_Config.Aimbot->Enabled);
			RegisterBool (xorstr("Combat:Aimbot:Globals:Show Fov"),        &g_Config.Aimbot->ShowFov);
			RegisterBool (xorstr("Combat:Aimbot:Globals:Only Visible"),    &g_Config.Aimbot->OnlyVisible);
			RegisterBool (xorstr("Combat:Aimbot:Globals:Ignore NPCs"),     &g_Config.Aimbot->IgnoreNPCs);
			RegisterBool (xorstr("Combat:Aimbot:Globals:Prediction"),      &g_Config.Aimbot->Prediction);
			RegisterFloat(xorstr("Combat:Aimbot:Settings:Field of View"),  &g_Config.Aimbot->FOV);
			RegisterInt  (xorstr("Combat:Aimbot:Settings:Max Distance"),   &g_Config.Aimbot->MaxDistance);
			RegisterInt  (xorstr("Combat:Aimbot:Settings:Smooth Horizontal"), &g_Config.Aimbot->SmoothHorizontal);
			RegisterInt  (xorstr("Combat:Aimbot:Settings:Smooth Vertical"),   &g_Config.Aimbot->SmoothVertical);
			RegisterKeyBind(xorstr("Combat:Aimbot:Settings:Key Bind"),     &g_Config.Aimbot->KeyBind);
		}

		void RegisterTriggerBotConfigs()
		{
			RegisterBool (xorstr("Combat:TriggerBot:Globals:Toggle"),       &g_Config.TriggerBot->Enabled);
			RegisterBool (xorstr("Combat:TriggerBot:Globals:Show Fov"),     &g_Config.TriggerBot->ShowFov);
			RegisterBool (xorstr("Combat:TriggerBot:Globals:Only Visible"), &g_Config.TriggerBot->OnlyVisible);
			RegisterBool (xorstr("Combat:TriggerBot:Globals:Ignore NPCs"),  &g_Config.TriggerBot->IgnoreNPCs);
			RegisterBool (xorstr("Combat:TriggerBot:Globals:Smart Trigger"),&g_Config.TriggerBot->SmartTrigger);
			RegisterFloat(xorstr("Combat:TriggerBot:Settings:Field of View"),&g_Config.TriggerBot->FOV);
			RegisterInt  (xorstr("Combat:TriggerBot:Settings:Max Distance"), &g_Config.TriggerBot->MaxDistance);
			RegisterInt  (xorstr("Combat:TriggerBot:Settings:Delay"),        &g_Config.TriggerBot->Delay);
			RegisterKeyBind(xorstr("Combat:TriggerBot:Settings:Key Bind"),   &g_Config.TriggerBot->KeyBind);
		}

		void RegisterSilentAimConfigs()
		{
			RegisterBool (xorstr("Combat:SilentAim:Globals:Toggle"),        &g_Config.SilentAim->Enabled);
			RegisterBool (xorstr("Combat:SilentAim:Globals:Show Fov"),      &g_Config.SilentAim->ShowFov);
			RegisterBool (xorstr("Combat:SilentAim:Globals:Only Visible"),  &g_Config.SilentAim->OnlyVisible);
			RegisterBool (xorstr("Combat:SilentAim:Globals:Ignore NPCs"),   &g_Config.SilentAim->IgnoreNPCs);
			RegisterBool (xorstr("Combat:SilentAim:Globals:Magic Bullets"), &g_Config.SilentAim->MagicBullets);
			RegisterFloat(xorstr("Combat:SilentAim:Settings:Field of View"),&g_Config.SilentAim->FOV);
			RegisterInt  (xorstr("Combat:SilentAim:Settings:Max Distance"), &g_Config.SilentAim->MaxDistance);
			// FIX: MissChance is float — was wrongly registered as Int before
			RegisterFloat(xorstr("Combat:SilentAim:Settings:Miss Chance"),  &g_Config.SilentAim->MissChance);
			RegisterKeyBind(xorstr("Combat:SilentAim:Settings:Key Bind"),   &g_Config.SilentAim->KeyBind);
		}

		void RegisterESPConfigs()
		{
			RegisterBool(xorstr("Visuals:ESP:General:Toggle"),           &g_Config.ESP->Enabled);
			RegisterBool(xorstr("Visuals:ESP:General:Ignore NPCs"),      &g_Config.ESP->IgnoreNPCs);
			RegisterBool(xorstr("Visuals:ESP:General:Show Local Player"),&g_Config.ESP->ShowLocalPlayer);
			RegisterBool(xorstr("Visuals:ESP:General:Highlight Visible"),&g_Config.ESP->HighlightVisible);
			RegisterBool(xorstr("Visuals:ESP:General:Ignore Dead"),      &g_Config.ESP->IgnoreDead);
			RegisterBool(xorstr("Visuals:ESP:Players:Box"),              &g_Config.ESP->Box);
			RegisterBool(xorstr("Visuals:ESP:Players:Filled Box"),       &g_Config.ESP->FilledBox);
			RegisterBool(xorstr("Visuals:ESP:Players:Skeleton"),         &g_Config.ESP->Skeleton);
			RegisterBool(xorstr("Visuals:ESP:Players:Health Bar"),       &g_Config.ESP->HealthBar);
			RegisterInt (xorstr("Visuals:ESP:Players:Health Style"),     &g_Config.ESP->HealthStyle);
			RegisterBool(xorstr("Visuals:ESP:Players:Armor Bar"),        &g_Config.ESP->ArmorBar);
			RegisterInt (xorstr("Visuals:ESP:Players:Armor Style"),      &g_Config.ESP->ArmorStyle);
			RegisterBool(xorstr("Visuals:ESP:Players:Weapon Name"),      &g_Config.ESP->WeaponName);
			RegisterBool(xorstr("Visuals:ESP:Players:Snap Lines"),       &g_Config.ESP->SnapLines);
			RegisterBool(xorstr("Visuals:ESP:Players:User Names"),       &g_Config.ESP->UserNames);
			RegisterBool(xorstr("Visuals:ESP:Players:Head Circle"),      &g_Config.ESP->HeadCircle);
			RegisterBool(xorstr("Visuals:ESP:Players:Distance From Me"), &g_Config.ESP->DistanceFromMe);
			RegisterBool(xorstr("Visuals:ESP:Players:Friends Marker"),   &g_Config.ESP->FriendsMarker);
			RegisterInt (xorstr("Visuals:ESP:Settings:Max Distance"),    &g_Config.ESP->MaxDistance);
			RegisterBool(xorstr("Visuals:Radar:Enabled"),                &g_Config.ESP->RadarEnabled);
			RegisterBool(xorstr("Visuals:Radar:Detached"),               &g_Config.ESP->RadarDetached);
		}

		void RegisterVehicleESPConfigs()
		{
			RegisterBool(xorstr("Visuals:VehicleESP:Toggle"),          &g_Config.VehicleESP->Enabled);
			RegisterBool(xorstr("Visuals:VehicleESP:Snap Lines"),      &g_Config.VehicleESP->SnapLines);
			RegisterBool(xorstr("Visuals:VehicleESP:Show Lock Unlock"),&g_Config.VehicleESP->ShowLockUnlock);
			RegisterBool(xorstr("Visuals:VehicleESP:Vehicle Name"),    &g_Config.VehicleESP->VehName);
			RegisterBool(xorstr("Visuals:VehicleESP:Vehicle Health"),  &g_Config.VehicleESP->VehHealth);
			RegisterBool(xorstr("Visuals:VehicleESP:Distance From Me"),&g_Config.VehicleESP->DistanceFromMe);
			RegisterInt (xorstr("Visuals:VehicleESP:Max Distance"),    &g_Config.VehicleESP->MaxDistance);
		}

		void RegisterPlayerConfigs()
		{
			RegisterBool    (xorstr("Local:General:Fast Run"),          &g_Config.Player->FastRun);
			RegisterFloat   (xorstr("Local:General:Run Speed"),         &g_Config.Player->RunSpeed);
			RegisterBool    (xorstr("Local:General:Infinite Stamina"),  &g_Config.Player->InfiniteStamina);
			RegisterBool    (xorstr("Local:General:Invisible"),         &g_Config.Player->Invisible);
			RegisterBool    (xorstr("Local:God Mode:Enable"),           &g_Config.Player->EnableGodMode);
			RegisterKeyBind (xorstr("Local:God Mode:Key Bind"),         &g_Config.Player->GodModeKey);
			RegisterBool    (xorstr("Local:God Mode:Vehicle God Mode"), &g_Config.Player->VehicleGodMode);
			RegisterBool    (xorstr("Local:No Clip:Enable"),            &g_Config.Player->NoClipEnabled);
			RegisterKeyBind (xorstr("Local:No Clip:Key Bind"),          &g_Config.Player->NoClipKey);
			RegisterFloat   (xorstr("Local:No Clip:Speed"),             &g_Config.Player->NoClipSpeed);
			RegisterBool    (xorstr("Local:Weapons:No Recoil"),         &g_Config.Player->NoRecoilEnabled);
			RegisterBool    (xorstr("Local:Weapons:No Spread"),         &g_Config.Player->NoSpreadEnabled);
			RegisterBool    (xorstr("Local:Weapons:Infinite Ammo"),     &g_Config.Player->InfiniteAmmoEnabled);
			RegisterBool    (xorstr("Local:Weapons:No Reload"),         &g_Config.Player->NoReloadEnabled);
			RegisterBool    (xorstr("Local:Vehicle:Seat Belt"),         &g_Config.Player->SeatBelt);
			RegisterBool    (xorstr("Local:Vehicle:No Ragdoll"),        &g_Config.Player->NoRagDollEnabled);
			RegisterBool    (xorstr("Local:Vehicle:Hornet Boost"),      &g_Config.Player->HornetBoost);
			RegisterBool    (xorstr("Local:Vehicle:Always Engine On"), &g_Config.Player->AlwaysEngineOn);
			RegisterBool    (xorstr("Local:Vehicle:Vehicle Jump"),      &g_Config.Player->VehicleJump);
		}

	public:
		// ---------------------------------------------------------------
		// Constructor / Destructor
		// ---------------------------------------------------------------
		cWebSocketSync()
			: m_webSocket(std::make_unique<ix::WebSocket>())
		{
			m_radarSnapshot.reserve( kMaxRadarPlayers );
			ix::initNetSystem();
		}

		~cWebSocketSync()
		{
			Disconnect();
		}

		// Non-copyable
		cWebSocketSync(const cWebSocketSync&)            = delete;
		cWebSocketSync& operator=(const cWebSocketSync&) = delete;

		// ---------------------------------------------------------------
		// Register helpers (public so external code can extend the map)
		// ---------------------------------------------------------------
		static std::string ClientUrl( ) { return ResolveUrl( ); }

		inline void RegisterBool    (const std::string& key, bool*  ptr) {
			std::lock_guard<std::mutex> Lock( m_registryMutex );
			m_configRegistry[key] = { ConfigType::Bool, ptr };
		}
		inline void RegisterInt     (const std::string& key, int*   ptr) {
			std::lock_guard<std::mutex> Lock( m_registryMutex );
			m_configRegistry[key] = { ConfigType::Int, ptr };
		}
		inline void RegisterFloat   (const std::string& key, float* ptr) {
			std::lock_guard<std::mutex> Lock( m_registryMutex );
			m_configRegistry[key] = { ConfigType::Float, ptr };
		}
		inline void RegisterKeyBind (const std::string& key, int*   ptr) {
			std::lock_guard<std::mutex> Lock( m_registryMutex );
			m_configRegistry[key] = { ConfigType::KeyBind, ptr };
		}

		// ---------------------------------------------------------------
		// Lifecycle
		// ---------------------------------------------------------------
		void Connect(const std::string& serverUrl)
		{
			InitializeRegistrations( );

			if ( m_isConnected.load( ) ) return;

			const std::string Url = serverUrl.empty( ) ? ResolveUrl( ) : serverUrl;
			SetUserName( g_Variables.UserName );
			m_serverUrl       = Url;
			m_shouldReconnect = true;
			m_needsRegister   = true;

			// setThreadName() usa RaiseException(0x406D1388). Em processos
			// injetados a cadeia SEH da thread pode nao estar pronta e essa
			// excecao "de debugger" termina o processo. O nome da thread e
			// apenas cosmético para debuggers, então desligamos.
			m_webSocket->setAutoThreadName( false );

			m_webSocket->setUrl( Url );
			m_webSocket->setOnMessageCallback( [ this ]( const ix::WebSocketMessagePtr & msg ) {
				OnMessageReceived( msg );
			} );

			// Sem compressao e sem TLS/certificado: e um ws:// simples.
			ix::WebSocketPerMessageDeflateOptions deflate( false );
			m_webSocket->setPerMessageDeflateOptions( deflate );

			if ( !m_radarRunning.exchange( true ) )
				m_radarThread = std::thread( [ this ]( ) { RadarLoop( ); } );

			try { m_webSocket->start( ); }
			catch ( ... ) {}
		}

		void Disconnect()
		{
			m_shouldReconnect  = false;
			m_reconnectPending = false;
			m_radarRunning     = false;
			m_reconnectWake.notify_all( );
			m_radarWake.notify_all( );

			if ( m_reconnectThread.joinable( ) ) {
				try { m_reconnectThread.join( ); }
				catch ( ... ) {}
			}

			if ( m_radarThread.joinable( ) ) {
				try { m_radarThread.join( ); }
				catch ( ... ) {}
			}

			if ( m_webSocket )
			{
				try
				{
					m_webSocket->stop();
					m_isConnected = false;
				}
				catch (...) {}
			}
		}

		// ---------------------------------------------------------------
		// Accessors
		// ---------------------------------------------------------------
		bool IsConnected() const { return m_isConnected; }

		// Send an arbitrary text message to the server
		bool Send(const std::string& message)
		{
			if (!m_isConnected.load( ) || !m_webSocket) return false;
			std::lock_guard<std::mutex> Lock( m_sendMutex );
			try
			{
				m_webSocket->send(message);
				return true;
			}
			catch (...) { return false; }
		}

		std::unordered_map<int, RemotePlayerData> GetRemotePlayersSnapshot() const
		{
			std::lock_guard<std::mutex> lock(m_remotePlayersMutex);
			return m_remotePlayersData;
		}

		// Returns nullptr if player is not in the map
		// NOTE: pointer is valid only while the caller holds no lock — copy the value out if needed
		bool GetRemotePlayer(int playerId, RemotePlayerData& out) const
		{
			std::lock_guard<std::mutex> lock(m_remotePlayersMutex);
			auto it = m_remotePlayersData.find(playerId);
			if (it == m_remotePlayersData.end()) return false;
			out = it->second;
			return true;
		}

		// Evict entries older than `maxAgeMs` milliseconds (call from game thread periodically)
		void PurgeStaleRadarEntries(int64_t maxAgeMs = 5000)
		{
			int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now().time_since_epoch()).count();

			std::lock_guard<std::mutex> lock(m_remotePlayersMutex);
			for (auto it = m_remotePlayersData.begin(); it != m_remotePlayersData.end(); )
			{
				if ((now - it->second.LastUpdate) > maxAgeMs)
					it = m_remotePlayersData.erase(it);
				else
					++it;
			}
		}
	};

	// -------------------------------------------------------------------
	// Global instance + free-function helpers
	// -------------------------------------------------------------------
	inline std::unique_ptr<cWebSocketSync> g_WebSocketSync;

	// URL explicita, ou ws://weedsolution.xyz:1534 com token opcional.
	// Nao usa TLS: nao ha certificado disponivel para o canal do C++.
	inline void InitializeWebSocket(const std::string& serverUrl = {})
	{
		if ( g_WebSocketSync && g_WebSocketSync->IsConnected( ) )
			return;

		if ( g_WebSocketSync ) {
			g_WebSocketSync->Disconnect( );
			g_WebSocketSync.reset( );
		}

		const std::string Url = serverUrl.empty( ) ? cWebSocketSync::ClientUrl( ) : serverUrl;
		if ( Url.empty( ) ) return;

		g_WebSocketSync = std::make_unique<cWebSocketSync>( );
		g_WebSocketSync->Connect( Url );
	}

	inline void ShutdownWebSocket()
	{
		if (g_WebSocketSync)
		{
			g_WebSocketSync->Disconnect();
			g_WebSocketSync.reset();
		}
	}

	inline bool IsWebSocketConnected()
	{
		return g_WebSocketSync && g_WebSocketSync->IsConnected();
	}

	// Convenience wrapper — broadcast a text frame if connected
	inline bool WebSocketSend(const std::string& message)
	{
		return g_WebSocketSync && g_WebSocketSync->Send(message);
	}

} // namespace Core::Threads
