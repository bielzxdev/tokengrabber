#pragma once

#include <Includes/Includes.hpp>
#include <shlobj_core.h>
#include <fstream>
#include <sddl.h>
#include <random>
#include <regex>
#include <TlHelp32.h>

namespace Utils {

	enum class Language
	{
		PortugueseBR,
		EnglishUK,
		EnglishUS,
		Russian,
		Spanish
	};

	// Portuguese is the default; English is the canonical key language.
	inline Language g_CurrentLanguage = Language::PortugueseBR;

	// xorstr() decrypts into the internal buffer of a temporary xor_string object. That temporary
	// is destroyed at the end of the full expression, so a `return xorstr("...")` (the previous
	// implementation) handed callers a pointer that was already dangling by the time they read it -
	// this is exactly what corrupted the on-screen text into garbled "??" runs, worse the more
	// frames/strings were drawn. Store() copies the decrypted text into one of a few static
	// buffers that stay alive for the rest of the frame, so the pointer Translate() returns is
	// always valid when ImGui actually renders it.
	inline const char * Translate_Store( const char * Decrypted )
	{
		static thread_local char Buffers[ 64 ][ 512 ];
		static thread_local int BufferIndex = 0;

		char * Buffer = Buffers[ BufferIndex ];
		BufferIndex = ( BufferIndex + 1 ) % 64;

		if ( !Decrypted )
			return Translate_Store( xorstr( "" ) );

		strncpy_s( Buffer, sizeof( Buffers[ 0 ] ), Decrypted, _TRUNCATE );
		return Buffer;
	}

	inline const char* Translate( const char* key )
	{
		if ( !key )
			return Translate_Store( xorstr( "" ) );

		// O catálogo antigo de russo e espanhol foi criado antes de vários
		// cards novos. Resolva aqui as chaves compartilhadas para que esses
		// idiomas não caiam em textos portugueses nem em ponteiros temporários.
		// Os nomes dos parametros NAO podem ser Russian/Spanish: o pre-processador
		// substituiria o token dentro de `Language::Russian`, gerando
		// `Language::"Konfiguratsiya"` e C2589. Use nomes neutros e deixe os
		// enumeradores literais.
		#define RU_ES( EngText, RusText, EspText ) \
			if ( g_CurrentLanguage == Language::Russian && strcmp( key, xorstr( EngText ) ) == 0 ) return Translate_Store( xorstr( RusText ) ); \
			if ( g_CurrentLanguage == Language::Spanish && strcmp( key, xorstr( EngText ) ) == 0 ) return Translate_Store( xorstr( EspText ) )

		RU_ES( "Configuration", "Konfiguratsiya", "Configuración" );
		RU_ES( "WebRadar", "WebRadar", "WebRadar" );
		RU_ES( "Radar", "Radar", "Radar" );
		RU_ES( "CONNECTION", "SOEDINENIE", "CONEXIÓN" );
		RU_ES( "LOCAL PLAYER", "LOKALNYI IGROK", "JUGADOR LOCAL" );
		RU_ES( "PLAYERS", "IGROKI", "JUGADORES" );
		RU_ES( "Status", "Status", "Estado" );
		RU_ES( "Online", "Online", "En línea" );
		RU_ES( "Offline", "Offline", "Desconectado" );
		RU_ES( "You", "Vy", "Tú" );
		RU_ES( "Friendly", "Drug", "Amigo" );
		RU_ES( "Visible", "Vidimyi", "Visible" );
		RU_ES( "Hidden", "Skrytyi", "Oculto" );
		RU_ES( "Waiting for data...", "Ozhidanie dannyh...", "Esperando datos..." );
		RU_ES( "scroll to zoom", "prokrutka dlya uvelicheniya", "desplaza para ampliar" );
		RU_ES( "Enable", "Vklyuchit", "Activar" );
		RU_ES( "Show FOV", "Pokazat FOV", "Mostrar FOV" );
		RU_ES( "Prediction", "Predskazanie", "Predicción" );
		RU_ES( "Smooth Horizontal", "Gorizontalnoe Sglazhivanie", "Suavidad horizontal" );
		RU_ES( "Smooth Vertical", "Vertikalnoe Sglazhivanie", "Suavidad vertical" );
		RU_ES( "Smart Trigger", "Umnyi Triggerbot", "Triggerbot inteligente" );
		RU_ES( "Magnetto", "Magnetto", "Magnetto" );
		RU_ES( "Range", "Rasstoyanie", "Alcance" );
		RU_ES( "Freecam", "Svobodnaya Kamera", "Cámara libre" );
		RU_ES( "Shotgun", "Drobovik", "Escopeta" );
		RU_ES( "Rifle", "Vintovka", "Rifle" );
		RU_ES( "Machine Gun", "Pulemet", "Ametralladora" );
		RU_ES( "Sniper", "Snaiper", "Francotirador" );
		RU_ES( "Heavy", "Tyazheloe", "Pesada" );
		RU_ES( "Throwable", "Metatelnoe", "Lanzable" );
		RU_ES( "Paleto Bay", "Paleto Bay", "Paleto Bay" );
		RU_ES( "Cassino", "Kazino", "Casino" );
		RU_ES( "Overlay Monitor", "Monitor Overlay", "Monitor del overlay" );
		RU_ES( "Overlay placement", "Pozitsiya Overlay", "Posición del overlay" );
		RU_ES( "Use selected monitor", "Ispolzovat vybrannyi monitor", "Usar monitor seleccionado" );
		RU_ES( "No monitor found", "Monitor ne naiden", "No se encontró ningún monitor" );
		RU_ES( "Set Monitor", "Ustanovit monitor", "Aplicar monitor" );
		RU_ES( "Monitor applied", "Monitor ustanovlen", "Monitor aplicado" );
		RU_ES( "Selected", "Vybrano", "Seleccionado" );
		RU_ES( "My Configs", "Moi Konfiguratsii", "Mis configuraciones" );
		RU_ES( "Your cloud configs", "Moi oblachnye konfiguratsii", "Tus configuraciones en la nube" );
		RU_ES( "Login to manage your configs.", "Voiti dlya upravleniya konfiguratsiyami.", "Inicia sesión para administrar tus configuraciones." );
		RU_ES( "User", "Polzovatel", "Usuario" );
		RU_ES( "No cloud configs found.", "Oblachnye konfiguratsii ne naideny.", "No se encontraron configuraciones en la nube." );
		RU_ES( "Selected config", "Vybrannaya konfiguratsiya", "Configuración seleccionada" );
		RU_ES( "Config name", "Imya konfiguratsii", "Nombre de configuración" );
		RU_ES( "Rename config", "Pereimenovat konfiguratsiyu", "Renombrar configuración" );
		RU_ES( "New name", "Novoe imya", "Nuevo nombre" );
		RU_ES( "Cloud actions", "Deistviya v oblake", "Acciones en la nube" );
		RU_ES( "Save Cloud", "Sohranit v oblako", "Guardar en la nube" );
		RU_ES( "Load Cloud", "Zagruzit iz oblaka", "Cargar desde la nube" );
		RU_ES( "Rename Cloud", "Pereimenovat v oblake", "Renombrar en la nube" );
		RU_ES( "Delete Cloud", "Udalit iz oblaka", "Eliminar de la nube" );
		RU_ES( "Refresh", "Obnovit", "Actualizar" );
		RU_ES( "Working...", "Rabotaem...", "Procesando..." );
		RU_ES( "Interface", "Interfeis", "Interfaz" );
		RU_ES( "Accent Color", "Aktsentnyi tsvet", "Color de acento" );
		RU_ES( "Accent Light", "Svetlyi aktsent", "Acento claro" );
		RU_ES( "Accent Dark", "Temnyi aktsent", "Acento oscuro" );
		RU_ES( "Accent Fill", "Zalivka aktsenta", "Relleno de acento" );
		RU_ES( "Armor Display", "Otobrazhenie Bronii", "Mostrar armadura" );
		RU_ES( "Id:", "ID:", "ID:" );
		RU_ES( "Freeze Vehicle", "Zamorozit transport", "Congelar vehículo" );
		RU_ES( "Unfreeze Vehicle", "Razmorozit transport", "Descongelar vehículo" );
		RU_ES( "Teleport to Ocean", "Teleport v okean", "Teletransportar al océano" );
		RU_ES( "Shows or hides the radar.", "Pokazyvaet ili skryvaet radar.", "Muestra u oculta el radar." );
		RU_ES( "Detaches the radar from the card and leaves it free on the overlay.", "Otvyazyvaet radar ot kartochki i razmeschaet ego na overlay.", "Separa el radar de la tarjeta y lo deja libre en el overlay." );
		RU_ES( "Enable Radar", "Vklyuchit Radar", "Activar radar" );
		RU_ES( "Detach Radar", "Otvyazat Radar", "Separar radar" );
		RU_ES( "Reset Radar Position", "Sbrosit pozitsiyu radara", "Restablecer posición del radar" );
		RU_ES( "Radar detached: drag the title bar to move it.", "Radar otvyazan: peretaskivayte zagolovok dlya peremescheniya.", "Radar separado: arrastra la barra de título para moverlo." );
		RU_ES( "Radar disabled", "Radar otklyuchen", "Radar desactivado" );
		RU_ES( "Remove Driver", "Udalit voditelya", "Eliminar conductor" );
		RU_ES( "Eject Everyone", "Vybrosit vseh", "Expulsar a todos" );
		RU_ES( "Stop Spectate", "Ostanovit prosmotr", "Detener espectador" );
		RU_ES( "Spectate", "Smotret", "Observar" );
		RU_ES( "Magnetto (Pull)", "Magnetto (Tyunut)", "Magnetto (Atraer)" );
		RU_ES( "Throw Car", "Brosit mashinu", "Lanzar coche" );
		RU_ES( "Launch Up", "Podnyat vverh", "Lanzar hacia arriba" );
		RU_ES( "Send to Ocean", "Otpravit v okean", "Enviar al océano" );
		RU_ES( "Freecam Actions", "Deistviya Svobodnoi Kamery", "Acciones de cámara libre" );
		RU_ES( "Vehicle Detected", "Transport obnaruzhen", "Vehículo detectado" );
		RU_ES( "Distance: %.0fm", "Rasstoyanie: %.0fm", "Distancia: %.0fm" );
		RU_ES( "Use Arrows: UP/DOWN | Enter: Execute", "Ispolzuyte strelki: VVERH/NIZ | Enter: Vypolnit", "Usa las flechas: ARRIBA/ABAJO | Enter: Ejecutar" );

		#undef RU_ES

		switch ( g_CurrentLanguage )
		{
		case Language::PortugueseBR:
		{
			#define PT( English, Portuguese ) if ( strcmp( key, xorstr( English ) ) == 0 ) return Translate_Store( xorstr( Portuguese ) )

			PT( "Combat", "Assistência de mira" );
			PT( "Visuals", "Visuais" );
			PT( "Local", "Jogador" );
			PT( "World", "Mundo" );
			PT( "Scripts", "Scripts" );
			PT( "Settings", "Outros" );
			PT( "Players", "Jogadores" );
			PT( "Vehicles", "Veículos" );
			PT( "Main", "Principal" );
			PT( "Teleports", "Teleportes" );
			PT( "Weapons", "Armas" );
			PT( "Colors", "Cores" );
			PT( "General", "Geral" );
			PT( "Globals", "Ativação" );
			PT( "Preview", "Prévia" );
			PT( "Actions", "Ações" );
			PT( "Locations", "Locais" );
			PT( "Info", "Informações" );
			PT( "Configs", "Configurações" );
			PT( "Menu Preferences", "Preferências do menu" );
			PT( "Configuration Actions", "Ações de configuração" );
			PT( "Player Features", "Recursos do jogador" );
			PT( "Health and Armor Actions", "Ações de vida e colete" );
			PT( "Vehicle Features", "Recursos do veículo" );
			PT( "Vehicle Actions", "Ações do veículo" );
			PT( "Player Visuals", "Visuais dos jogadores" );
			PT( "Vehicle Visuals", "Visuais dos veículos" );
			PT( "Weapon Modifiers", "Modificadores da arma" );
			PT( "Ammunition Actions", "Ações de munição" );
			PT( "Weapon Attachments", "Acessórios da arma" );
			PT( "Aimbot Settings", "Configurações do Aimbot" );
			PT( "TriggerBot Settings", "Configurações do TriggerBot" );
			PT( "SilentAim Settings", "Configurações do SilentAim" );
			PT( "Aimbot", "Aimbot" );
			PT( "TriggerBot", "TriggerBot" );
			PT( "SilentAim", "SilentAim" );
			PT( "Display", "Exibição" );
			PT( "Here you can spawn weapon for you", "Escolha uma arma para adicionar" );
			PT( "License validation", "Validação de licença" );
			PT( "Validating... Please wait", "Validando... Aguarde" );
			PT( "Enter your license key or 'DWDWDW' for dev mode", "Digite sua chave de licença ou 'DWDWDW' para o modo de desenvolvimento" );
			PT( "License key", "Chave de licença" );
			PT( "Validate", "Validar" );
			PT( "Account login", "Login da conta" );
			PT( "Enter your account username and password", "Digite seu usuário e senha" );
			PT( "Username", "Usuário" );
			PT( "Password", "Senha" );
			PT( "Login", "Entrar" );
			PT( "Exit", "Sair" );
			PT( "Authentication failed", "Falha na autenticação" );

			PT( "Enable Aimbot", "Ativar Aimbot" );
			PT( "Enable TriggerBot", "Ativar TriggerBot" );
			PT( "Enable SilentAim", "Ativar SilentAim" );
			PT( "Enable Visuals", "Ativar visuais" );
			PT( "Enable Vehicle ESP", "Ativar ESP de veículos" );
			PT( "WebRadar", "WebRadar" );
			PT( "Radar", "Radar" );
			PT( "Enable Radar", "Ativar radar" );
			PT( "Detach Radar", "Desalocar radar" );
			PT( "Reset Radar Position", "Redefinir posição do radar" );
			PT( "Radar detached: drag the title bar to move it.", "Radar desalocado: arraste a barra de título para movê-lo." );
			PT( "Radar disabled", "Radar desativado" );
			PT( "CONNECTION", "CONEXÃO" );
			PT( "LOCAL PLAYER", "JOGADOR LOCAL" );
			PT( "PLAYERS", "JOGADORES" );
			PT( "Status", "Status" );
			PT( "Online", "Online" );
			PT( "Offline", "Offline" );
			PT( "You", "Você" );
			PT( "Friendly", "Amigo" );
			PT( "Hidden", "Oculto" );
			PT( "Waiting for data...", "Aguardando dados..." );
			PT( "scroll to zoom", "role para ampliar" );
			PT( "Bind", "Tecla" );
			PT( "Sticky Target", "Manter alvo" );
			PT( "Show Fov", "Mostrar FOV" );
			PT( "Only Visible", "Apenas visíveis" );
			PT( "Ignore NPCs", "Ignorar NPCs" );
			PT( "Ignore Dead", "Ignorar mortos" );
			PT( "Show LocalPlayer", "Mostrar jogador local" );
			PT( "Field of View", "Campo de visão" );
			PT( "Smooth X", "Suavização horizontal" );
			PT( "Smooth Y", "Suavização vertical" );
			PT( "Max Distance", "Distância máxima" );
			PT( "Smart Triggerbot", "Triggerbot inteligente" );
			PT( "Delay", "Atraso" );
			PT( "Magic Bullets", "Balas mágicas" );
			PT( "Miss Chance", "Chance de errar" );
			PT( "Hitbox", "Região do corpo" );
			PT( "Head", "Cabeça" );
			PT( "Neck", "Pescoço" );
			PT( "Torso", "Tronco" );
			PT( "Target Vehicle Tires", "Mirar nos pneus dos veículos" );

			PT( "Box", "Caixa" );
			PT( "Filled Box", "Caixa preenchida" );
			PT( "Skeleton", "Esqueleto" );
			PT( "SnapLines", "Linhas de direção" );
			PT( "Names", "Nomes" );
			PT( "Player Names", "Nomes dos jogadores" );
			PT( "Weapon Names", "Nomes das armas" );
			PT( "Vehicle Names", "Nomes dos veículos" );
			PT( "Distance", "Distância" );
			PT( "Head Circle", "Círculo da cabeça" );
			PT( "Health Bar", "Barra de vida" );
			PT( "Health Display", "Exibição da vida" );
			PT( "Bar", "Barra" );
			PT( "Armor Bar", "Barra de colete" );
			PT( "Highlight Visible", "Destacar visíveis" );
			PT( "Friend Marker", "Marcador de amigo" );
			PT( "Visible", "Visível" );
			PT( "Occluded", "Oculto" );
			PT( "Locked/Unlocked", "Trancado/Destrancado" );

			PT( "GodMode", "Modo Deus" );
			PT( "Car Throw", "Arremessar veículo" );
			PT( "Force", "Força" );
			PT( "Block Aiming", "Bloquear mira" );
			PT( "NoClip", "Noclip" );
			PT( "Speed", "Velocidade" );
			PT( "Fast Run", "Corrida rápida" );
			PT( "Anti Headshot", "Proteção contra headshot" );
			PT( "Shrink", "Encolher" );
			PT( "No RagDoll", "Sem queda" );
			PT( "Steal Car", "Roubar veículo" );
			PT( "Infinite CombatRoll", "Rolamento infinito" );
			PT( "Infinite Stamina", "Energia infinita" );
			PT( "Force Tab", "Forçar roda de armas" );
			PT( "Invisible", "Invisível" );
			PT( "Health", "Vida" );
			PT( "Armor", "Colete" );
			PT( "Set Health", "Definir vida" );
			PT( "Set Armor", "Definir colete" );
			PT( "Doors Locked", "Portas trancadas" );
			PT( "Don't Fall", "Não cair" );
			PT( "Handling Editor", "Editor de dirigibilidade" );
			PT( "Repair Vehicle", "Reparar veículo" );
			PT( "Bind Fix", "Tecla de reparo" );
			PT( "Bind Unlock", "Tecla de destravar" );
			PT( "Refill Rocket Boost", "Recarregar impulso" );
			PT( "Break Wheels", "Quebrar rodas" );
			PT( "Steal Seat", "Tomar assento" );
			PT( "Always Engine On", "Motor sempre ligado" );
			PT( "Hornet Boost", "Impulso do Hornet" );
			PT( "Gay Mode", "Modo colorido" );
			PT( "Armored Vehicle", "Veículo blindado" );
			PT( "Vehicle Health", "Resistência do veículo" );
			PT( "Perfect Tune", "Ajuste perfeito" );
			PT( "Install Preset (All)", "Instalar predefinição completa" );
			PT( "Restore Original Tune", "Restaurar ajuste original" );
			PT( "Install Armor Mod (Max)", "Instalar blindagem máxima" );
			PT( "Restore Original Armor", "Restaurar blindagem original" );
			PT( "Failed to apply tune", "Falha ao aplicar o ajuste" );
			PT( "Failed to apply armor", "Falha ao aplicar a blindagem" );
			PT( "Plate", "Placa" );

			PT( "Waypoint", "Destino marcado" );
			PT( "Square", "Praça" );
			PT( "Pier", "Píer" );
			PT( "Central Bank", "Banco Central" );
			PT( "Los Santos Airport", "Aeroporto de Los Santos" );
			PT( "Select a location to teleport.", "Selecione um local para teleportar." );
			PT( "Teleport", "Teleportar" );
			PT( "Teleport to ", "Teleportar para " );
			PT( "Custom Location", "Local personalizado" );
			PT( "Save Current Position", "Salvar posição atual" );
			PT( "Delete Selected Location", "Excluir local selecionado" );

			PT( "Recoil Control", "Controle de recuo" );
			PT( "Spread Control", "Controle de dispersão" );
			PT( "Range Control", "Controle de alcance" );
			PT( "Damage Control", "Controle de dano" );
			PT( "Size Control", "Controle de tamanho" );
			PT( "Velocity Control", "Controle de velocidade" );
			PT( "Rapid Fire", "Tiro rápido" );
			PT( "Infinite Ammo", "Munição infinita" );
			PT( "Ammo Value", "Quantidade de munição" );
			PT( "Add Ammo", "Adicionar munição" );
			PT( "Value", "Valor" );
			PT( "Get a weapon first", "Pegue uma arma primeiro" );
			PT( "Weapon Spawn", "Spawn de armas" );
			PT( "Give Weapon", "Adicionar arma" );
			PT( "Search weapons...", "Buscar armas..." );
			PT( "No weapons match your search", "Nenhuma arma encontrada" );
			PT( "Attach Spawner", "Spawn de acessórios" );
			PT( "Give Attach", "Adicionar acessório" );
			PT( "Search attachments...", "Buscar acessórios..." );
			PT( "No attachments match your search", "Nenhum acessório encontrado" );
			PT( "Select a weapon beside it", "Selecione uma arma ao lado" );
			PT( "Checking compatible components...", "Analisando componentes compatíveis..." );
			PT( "No compatible attachments", "Nenhum acessório compatível" );
			PT( "Remove All", "Remover todos" );
			PT( "Total", "Total" );
			PT( "Resources", "Recursos" );
			PT( "List of current server resources", "Lista de resources atuais do servidor" );
			PT( "No resources available", "Nenhum resource disponível" );
			PT( "AntiCheat (by resource name)", "AntiCheat (por nome de resource)" );
			PT( "Waiting for resources", "Aguardando resources" );
			PT( "Not identified by resource name", "Não identificado pelo nome dos resources" );
			PT( "Unknown product (generic name)", "Produto desconhecido (nome genérico)" );
			PT( "Name matches are estimates; renamed or server-only anticheats may not appear.", "Identificação estimada por nome; anticheats renomeados ou exclusivos do servidor podem não aparecer." );
			PT( "Started", "Iniciado" );
			PT( "Starting", "Iniciando" );
			PT( "Stopped", "Parado" );
			PT( "Stopping", "Parando" );

			PT( "Melee", "Corpo a corpo" );
			PT( "Handguns", "Pistolas" );
			PT( "Shotguns", "Escopetas" );
			PT( "Assault Rifles", "Fuzis de assalto" );
			PT( "Machine Guns", "Metralhadoras" );
			PT( "Sniper Rifles", "Rifles de precisão" );
			PT( "Heavy Weapons", "Armas pesadas" );
			PT( "Throwables", "Arremessáveis" );
			PT( "Misc", "Diversos" );

			PT( "Select Player", "Selecionar jogador" );
			PT( "Select Vehicle", "Selecionar veículo" );
			PT( "Search player...", "Buscar jogador..." );
			PT( "Search resources...", "Buscar resources..." );
			PT( "No resources match your search", "Nenhuma resource encontrada" );
			PT( "Other states", "Outros estados" );
			PT( "Name:", "Nome:" );
			PT( "Distance:", "Distância:" );
			PT( "Weapon:", "Arma:" );
			PT( "Friend:", "Amigo:" );
			PT( "Driver:", "Motorista:" );
			PT( "Doors Locked:", "Portas trancadas:" );
			PT( "Friend", "Amigo" );
			PT( "True", "Sim" );
			PT( "False", "Não" );
			PT( "Yes", "Sim" );
			PT( "No", "Não" );
			PT( "Unknown", "Desconhecido" );
			PT( "Unlock", "Destrancar" );
			PT( "Lock", "Trancar" );
			PT( "Copy Outfit", "Copiar roupa" );

			PT( "VSync", "Sincronização vertical" );
			PT( "Stream Mode", "Modo de transmissão" );
			PT( "Menu Key", "Tecla do menu" );
			PT( "Save Config", "Salvar configuração" );
			PT( "Load Config", "Carregar configuração" );
			PT( "Config saved", "Configuração salva" );
			PT( "Config loaded", "Configuração carregada" );
			PT( "Failed to save config", "Falha ao salvar a configuração" );
			PT( "No saved config found", "Nenhuma configuração salva encontrada" );
			PT( "Unload", "Descarregar" );
			PT( "Font Size", "Tamanho da fonte" );
			PT( "Letter Spacing", "Espaçamento das letras" );
			PT( "Text", "Texto" );
			PT( "Aimbot Fov", "FOV do Aimbot" );
			PT( "Triggerbot Fov", "FOV do Triggerbot" );
			PT( "SilentAim Fov", "FOV do SilentAim" );

			PT( "System Client Alert", "Aviso do sistema" );
			PT( "That function can be detected by server AntiCheat", "Essa função pode ser detectada pelo AntiCheat do servidor" );
			PT( "You must be in a car to use this function", "Você precisa estar em um veículo para usar esta função" );
			PT( "Hides this window from screen/stream capture", "Oculta esta janela de capturas e transmissões" );
			PT( "Stream Mode enabled", "Modo de transmissão ativado" );
			PT( "Stream Mode disabled", "Modo de transmissão desativado" );
			PT( "Failed to apply Stream Mode", "Falha ao ativar o modo de transmissão" );
			PT( "Failed to revert Stream Mode", "Falha ao desativar o modo de transmissão" );
			PT( " has been spawned.", " foi adicionada." );
			PT( " has been spawned", " foi adicionada" );
			PT( " failed to spawn.", " não pôde ser adicionada." );
			PT( " applied.", " aplicada." );
			PT( "Voltar", "Voltar" );
			PT( "Aceleração", "Aceleração" );
			PT( "Força de freio", "Força de freio" );
			PT( "Aderência mínima", "Aderência mínima" );

			// Chaves adicionadas depois do catálogo original. Todas as
			// strings que chegam à interface passam por este mesmo caminho,
			// inclusive as que foram criadas para o radar e para o overlay.
			PT( "Configuration", "Configuração" );
			PT( "Enable", "Ativar" );
			PT( "Show FOV", "Mostrar FOV" );
			PT( "Prediction", "Predição" );
			PT( "Smooth Horizontal", "Suavização horizontal" );
			PT( "Smooth Vertical", "Suavização vertical" );
			PT( "Smart Trigger", "Triggerbot inteligente" );
			PT( "Magnetto", "Magnetto" );
			PT( "Range", "Alcance" );
			PT( "Freecam", "Câmera livre" );
			PT( "Paleto Bay", "Paleto Bay" );
			PT( "Cassino", "Cassino" );
			PT( "Overlay Monitor", "Monitor do overlay" );
			PT( "Overlay placement", "Posicionamento do overlay" );
			PT( "Use selected monitor", "Usar monitor selecionado" );
			PT( "No monitor found", "Nenhum monitor encontrado" );
			PT( "Set Monitor", "Definir monitor" );
			PT( "Monitor applied", "Monitor aplicado" );
			PT( "Selected", "Selecionado" );
			PT( "My Configs", "Minhas configs" );
			PT( "Your cloud configs", "Suas configs na nuvem" );
			PT( "Login to manage your configs.", "Faça login para gerenciar suas configs." );
			PT( "User", "Usuário" );
			PT( "No cloud configs found.", "Nenhuma config na nuvem encontrada." );
			PT( "Selected config", "Config selecionada" );
			PT( "Config name", "Nome da config" );
			PT( "Rename config", "Renomear config" );
			PT( "New name", "Novo nome" );
			PT( "Cloud actions", "Ações na nuvem" );
			PT( "Save Cloud", "Salvar na nuvem" );
			PT( "Load Cloud", "Carregar da nuvem" );
			PT( "Rename Cloud", "Renomear na nuvem" );
			PT( "Delete Cloud", "Excluir da nuvem" );
			PT( "Refresh", "Atualizar" );
			PT( "Working...", "Processando..." );
			PT( "Interface", "Interface" );
			PT( "Accent Color", "Cor de destaque" );
			PT( "Accent Light", "Destaque claro" );
			PT( "Accent Dark", "Destaque escuro" );
			PT( "Accent Fill", "Preenchimento de destaque" );
			PT( "Armor Display", "Exibição do colete" );
			PT( "Id:", "ID:" );
			PT( "NPC", "NPC" );
			PT( "Freeze Vehicle", "Congelar veículo" );
			PT( "Unfreeze Vehicle", "Descongelar veículo" );
			PT( "Teleport to Ocean", "Teleportar para o oceano" );
			PT( "...", "..." );
			PT( "Weed Cheat", "Weed Cheat" );
			PT( "v1.0", "v1.0" );
			PT( "Remove Driver", "Remover motorista" );
			PT( "Eject Everyone", "Ejetar todos" );
			PT( "Stop Spectate", "Parar espectador" );
			PT( "Spectate", "Observar" );
			PT( "Magnetto (Pull)", "Magnetto (puxar)" );
			PT( "Throw Car", "Arremessar veículo" );
			PT( "Launch Up", "Jogar para o alto" );
			PT( "Send to Ocean", "Jogar no mar" );
			PT( "Freecam Actions", "Ações da câmera livre" );
			PT( "Vehicle Detected", "Veículo detectado" );
			PT( "Distance: %.0fm", "Distância: %.0fm" );
			PT( "Use Arrows: UP/DOWN | Enter: Execute", "Use as setas: CIMA/BAIXO | Enter: executar" );

			PT( "Shows or hides the radar.", "Mostra ou oculta o radar." );
			PT( "Detaches the radar from the card and leaves it free on the overlay.", "Desaloca o radar do card e o deixa livre no overlay." );

			PT( "Dagger", "Adaga" );
			PT( "Bat", "Taco" );
			PT( "Bottle", "Garrafa" );
			PT( "Crowbar", "Pé de cabra" );
			PT( "Flashlight", "Lanterna" );
			PT( "GolfClub", "Taco de golfe" );
			PT( "Hammer", "Martelo" );
			PT( "Hatchet", "Machadinha" );
			PT( "Knuckle", "Soco inglês" );
			PT( "Knife", "Faca" );
			PT( "Machete", "Facão" );
			PT( "Switchblade", "Canivete" );
			PT( "Nightstick", "Cassetete" );
			PT( "Wrench", "Chave inglesa" );
			PT( "BattleAxe", "Machado de batalha" );
			PT( "PoolCue", "Taco de sinuca" );
			PT( "StoneHatchet", "Machadinha de pedra" );
			PT( "Pistol", "Pistola" );
			PT( "PistolMk2", "Pistola Mk II" );
			PT( "CombatPistol", "Pistola de combate" );
			PT( "APPistol", "Pistola automática" );
			PT( "StunGun", "Arma de choque" );
			PT( "Pistol50", "Pistola .50" );
			PT( "SNSPistol", "Pistola SNS" );
			PT( "SNSPistolMk2", "Pistola SNS Mk II" );
			PT( "HeavyPistol", "Pistola pesada" );
			PT( "VintagePistol", "Pistola vintage" );
			PT( "FlareGun", "Pistola sinalizadora" );
			PT( "MarksmanPistol", "Pistola de atirador" );
			PT( "Revolver", "Revólver" );
			PT( "RevolverMk2", "Revólver Mk II" );
			PT( "DoubleAction", "Dupla ação" );
			PT( "RayPistol", "Pistola de raio" );
			PT( "CeramicPistol", "Pistola cerâmica" );
			PT( "NavyRevolver", "Revólver Navy" );
			PT( "MicroSMG", "Micro SMG" );
			PT( "SMG", "SMG" );
			PT( "SMGMk2", "SMG Mk II" );
			PT( "AssaultSMG", "SMG de assalto" );
			PT( "CombatPDW", "PDW de combate" );
			PT( "MachinePistol", "Pistola automática" );
			PT( "MiniSMG", "Mini SMG" );
			PT( "RayCarbine", "Carabina de raio" );
			PT( "PumpShotgun", "Escopeta pump" );
			PT( "PumpShotgunMk2", "Escopeta pump Mk II" );
			PT( "SawnoffShotgun", "Escopeta serrada" );
			PT( "AssaultShotgun", "Escopeta de assalto" );
			PT( "BullpupShotgun", "Escopeta bullpup" );
			PT( "Musket", "Mosquete" );
			PT( "HeavyShotgun", "Escopeta pesada" );
			PT( "AssaultRifle", "Fuzil de assalto" );
			PT( "AssaultRifleMk2", "Fuzil de assalto Mk II" );
			PT( "CarbineRifle", "Carabina" );
			PT( "CarbineRifleMk2", "Carabina Mk II" );
			PT( "AdvancedRifle", "Fuzil avançado" );
			PT( "SpecialCarbine", "Carabina especial" );
			PT( "SpecialCarbineMk2", "Carabina especial Mk II" );
			PT( "BullpupRifle", "Fuzil bullpup" );
			PT( "BullpupRifleMk2", "Fuzil bullpup Mk II" );
			PT( "CompactRifle", "Fuzil compacto" );
			PT( "MG", "Metralhadora" );
			PT( "CombatMG", "Metralhadora de combate" );
			PT( "CombatMGMk2", "Metralhadora de combate Mk II" );
			PT( "Gusenberg", "Gusenberg" );
			PT( "SniperRifle", "Rifle de precisão" );
			PT( "HeavySniper", "Rifle pesado" );
			PT( "HeavySniperMk2", "Rifle pesado Mk II" );
			PT( "MarksmanRifle", "Rifle de atirador" );
			PT( "MarksmanRifleMk2", "Rifle de atirador Mk II" );
			PT( "RPG", "RPG" );
			PT( "GrenadeLauncher", "Lança-granadas" );
			PT( "GrenadeLauncherSmoke", "Lança-granadas de fumaça" );
			PT( "Minigun", "Minigun" );
			PT( "Firework", "Lança-fogos" );
			PT( "Railgun", "Canhão elétrico" );
			PT( "HomingLauncher", "Lança-mísseis teleguiado" );
			PT( "CompactGrenadeLauncher", "Lança-granadas compacto" );
			PT( "RayMinigun", "Minigun de raio" );
			PT( "Grenade", "Granada" );
			PT( "BZGas", "Gás BZ" );
			PT( "Molotov", "Molotov" );
			PT( "StickyBomb", "Bomba adesiva" );
			PT( "ProximityMine", "Mina de proximidade" );
			PT( "Snowball", "Bola de neve" );
			PT( "PipeBomb", "Bomba caseira" );
			PT( "Ball", "Bola" );
			PT( "SmokeGrenade", "Granada de fumaça" );
			PT( "Flare", "Sinalizador" );
			PT( "PetrolCan", "Galão de gasolina" );
			PT( "Parachute", "Paraquedas" );
			PT( "FireExtinguisher", "Extintor" );
			PT( "Shotgun", "Escopeta" );
			PT( "Rifle", "Rifle" );
			PT( "Machine Gun", "Metralhadora" );
			PT( "Sniper", "Atirador" );
			PT( "Heavy", "Pesada" );
			PT( "Throwable", "Arremessável" );

			#undef PT
			return Translate_Store( key );
		}
		case Language::EnglishUK:
		{
			if ( strcmp( key, xorstr( "Combat" ) ) == 0 ) return Translate_Store( xorstr( "Combat" ) );
			if ( strcmp( key, xorstr( "Visuals" ) ) == 0 ) return Translate_Store( xorstr( "Visuals" ) );
			if ( strcmp( key, xorstr( "Local" ) ) == 0 ) return Translate_Store( xorstr( "Local" ) );
			if ( strcmp( key, xorstr( "World" ) ) == 0 ) return Translate_Store( xorstr( "World" ) );
			if ( strcmp( key, xorstr( "Exploits" ) ) == 0 ) return Translate_Store( xorstr( "Exploits" ) );
			if ( strcmp( key, xorstr( "Settings" ) ) == 0 ) return Translate_Store( xorstr( "Settings" ) );
			if ( strcmp( key, xorstr( "General" ) ) == 0 ) return Translate_Store( xorstr( "General" ) );
			if ( strcmp( key, xorstr( "Aimbot" ) ) == 0 ) return Translate_Store( xorstr( "Aimbot" ) );
			if ( strcmp( key, xorstr( "TriggerBot" ) ) == 0 ) return Translate_Store( xorstr( "TriggerBot" ) );
			if ( strcmp( key, xorstr( "SilentAim" ) ) == 0 ) return Translate_Store( xorstr( "SilentAim" ) );
			if ( strcmp( key, xorstr( "Players" ) ) == 0 ) return Translate_Store( xorstr( "Players" ) );
			if ( strcmp( key, xorstr( "Vehicles" ) ) == 0 ) return Translate_Store( xorstr( "Vehicles" ) );
			if ( strcmp( key, xorstr( "Main" ) ) == 0 ) return Translate_Store( xorstr( "Main" ) );
			if ( strcmp( key, xorstr( "Teleports" ) ) == 0 ) return Translate_Store( xorstr( "Teleports" ) );
			if ( strcmp( key, xorstr( "Weapons" ) ) == 0 ) return Translate_Store( xorstr( "Weapons" ) );
			if ( strcmp( key, xorstr( "Globals" ) ) == 0 ) return Translate_Store( xorstr( "Enable" ) );
			if ( strcmp( key, xorstr( "Bind" ) ) == 0 ) return Translate_Store( xorstr( "Bind" ) );
			if ( strcmp( key, xorstr( "Show Fov" ) ) == 0 ) return Translate_Store( xorstr( "Show FOV" ) );
			if ( strcmp( key, xorstr( "Only Visible" ) ) == 0 ) return Translate_Store( xorstr( "Only Visible" ) );
			if ( strcmp( key, xorstr( "Ignore NPCs" ) ) == 0 ) return Translate_Store( xorstr( "Ignore NPCs" ) );
			if ( strcmp( key, xorstr( "Globals Vector" ) ) == 0 ) return Translate_Store( xorstr( "Settings" ) );
			if ( strcmp( key, xorstr( "Field of View" ) ) == 0 ) return Translate_Store( xorstr( "Field of View" ) );
			if ( strcmp( key, xorstr( "Smooth" ) ) == 0 ) return Translate_Store( xorstr( "Smooth" ) );
			if ( strcmp( key, xorstr( "Max Distance" ) ) == 0 ) return Translate_Store( xorstr( "Max Distance" ) );
			if ( strcmp( key, xorstr( "Smart Triggerbot" ) ) == 0 ) return Translate_Store( xorstr( "Smart Triggerbot" ) );
			if ( strcmp( key, xorstr( "Delay" ) ) == 0 ) return Translate_Store( xorstr( "Delay" ) );
			if ( strcmp( key, xorstr( "Magic Bullets" ) ) == 0 ) return Translate_Store( xorstr( "Magic Bullets" ) );
			if ( strcmp( key, xorstr( "Miss Chance" ) ) == 0 ) return Translate_Store( xorstr( "Miss Chance" ) );
			if ( strcmp( key, xorstr( "VSync" ) ) == 0 ) return Translate_Store( xorstr( "VSync" ) );
			if ( strcmp( key, xorstr( "Stream Proof" ) ) == 0 ) return Translate_Store( xorstr( "Stream Proof" ) );
			if ( strcmp( key, xorstr( "Stream Mode" ) ) == 0 ) return Translate_Store( xorstr( "Stream Mode" ) );
			if ( strcmp( key, xorstr( "Menu Key" ) ) == 0 ) return Translate_Store( xorstr( "Menu Key" ) );
			if ( strcmp( key, xorstr( "Unload" ) ) == 0 ) return Translate_Store( xorstr( "Unload" ) );
			if ( strcmp( key, xorstr( "Config Vector" ) ) == 0 ) return Translate_Store( xorstr( "Configurations" ) );
			if ( strcmp( key, xorstr( "Export Config" ) ) == 0 ) return Translate_Store( xorstr( "Export Config" ) );
			if ( strcmp( key, xorstr( "Import Config" ) ) == 0 ) return Translate_Store( xorstr( "Import Config" ) );
			if ( strcmp( key, xorstr( "Colors" ) ) == 0 ) return Translate_Store( xorstr( "Colours" ) );
			if ( strcmp( key, xorstr( "Aimbot Fov" ) ) == 0 ) return Translate_Store( xorstr( "Aimbot FOV" ) );
			if ( strcmp( key, xorstr( "Triggerbot Fov" ) ) == 0 ) return Translate_Store( xorstr( "Triggerbot FOV" ) );
			if ( strcmp( key, xorstr( "SilentAim Fov" ) ) == 0 ) return Translate_Store( xorstr( "SilentAim FOV" ) );
			if ( strcmp( key, xorstr( "Box" ) ) == 0 ) return Translate_Store( xorstr( "Box" ) );
			if ( strcmp( key, xorstr( "Filled Box" ) ) == 0 ) return Translate_Store( xorstr( "Filled Box" ) );
			if ( strcmp( key, xorstr( "Skeleton" ) ) == 0 ) return Translate_Store( xorstr( "Skeleton" ) );
			if ( strcmp( key, xorstr( "SnapLines" ) ) == 0 ) return Translate_Store( xorstr( "SnapLines" ) );
			if ( strcmp( key, xorstr( "Names" ) ) == 0 ) return Translate_Store( xorstr( "Names" ) );
			if ( strcmp( key, xorstr( "Weapon Names" ) ) == 0 ) return Translate_Store( xorstr( "Weapon Names" ) );
			if ( strcmp( key, xorstr( "Distance" ) ) == 0 ) return Translate_Store( xorstr( "Distance" ) );
			if ( strcmp( key, xorstr( "GodMode" ) ) == 0 ) return Translate_Store( xorstr( "God Mode" ) );
			if ( strcmp( key, xorstr( "Block Aiming" ) ) == 0 ) return Translate_Store( xorstr( "Block Aiming" ) );
			if ( strcmp( key, xorstr( "NoClip" ) ) == 0 ) return Translate_Store( xorstr( "Noclip" ) );
			if ( strcmp( key, xorstr( "Speed" ) ) == 0 ) return Translate_Store( xorstr( "Speed" ) );
			if ( strcmp( key, xorstr( "Fast Run" ) ) == 0 ) return Translate_Store( xorstr( "Fast Run" ) );
			if ( strcmp( key, xorstr( "Anti Headshot" ) ) == 0 ) return Translate_Store( xorstr( "Anti Headshot" ) );
			if ( strcmp( key, xorstr( "Shrink" ) ) == 0 ) return Translate_Store( xorstr( "Shrink" ) );
			if ( strcmp( key, xorstr( "No RagDoll" ) ) == 0 ) return Translate_Store( xorstr( "No RagDoll" ) );
			if ( strcmp( key, xorstr( "Steal Car" ) ) == 0 ) return Translate_Store( xorstr( "Steal Car" ) );
			if ( strcmp( key, xorstr( "Infinite CombatRoll" ) ) == 0 ) return Translate_Store( xorstr( "Infinite Combat Roll" ) );
			if ( strcmp( key, xorstr( "Infinite Stamina" ) ) == 0 ) return Translate_Store( xorstr( "Infinite Stamina" ) );
			if ( strcmp( key, xorstr( "Force Tab" ) ) == 0 ) return Translate_Store( xorstr( "Force Tab" ) );
			if ( strcmp( key, xorstr( "Invisible" ) ) == 0 ) return Translate_Store( xorstr( "Invisible" ) );
			if ( strcmp( key, xorstr( "Health" ) ) == 0 ) return Translate_Store( xorstr( "Health" ) );
			if ( strcmp( key, xorstr( "Armor" ) ) == 0 ) return Translate_Store( xorstr( "Armour" ) );
			if ( strcmp( key, xorstr( "Doors Locked" ) ) == 0 ) return Translate_Store( xorstr( "Doors Locked" ) );
			if ( strcmp( key, xorstr( "Don't Fall" ) ) == 0 ) return Translate_Store( xorstr( "Don't Fall" ) );
			if ( strcmp( key, xorstr( "Handling Editor" ) ) == 0 ) return Translate_Store( xorstr( "Handling Editor" ) );
			if ( strcmp( key, xorstr( "Actions" ) ) == 0 ) return Translate_Store( xorstr( "Actions" ) );
			if ( strcmp( key, xorstr( "Repair Vehicle" ) ) == 0 ) return Translate_Store( xorstr( "Repair Vehicle" ) );
			if ( strcmp( key, xorstr( "Bind Fix" ) ) == 0 ) return Translate_Store( xorstr( "Bind Fix" ) );
			if ( strcmp( key, xorstr( "Bind Unlock" ) ) == 0 ) return Translate_Store( xorstr( "Bind Unlock" ) );
			if ( strcmp( key, xorstr( "Waypoint" ) ) == 0 ) return Translate_Store( xorstr( "Waypoint" ) );
			if ( strcmp( key, xorstr( "Square" ) ) == 0 ) return Translate_Store( xorstr( "Plaza" ) );
			if ( strcmp( key, xorstr( "Pier" ) ) == 0 ) return Translate_Store( xorstr( "Pier" ) );
			if ( strcmp( key, xorstr( "Paleto Bay" ) ) == 0 ) return Translate_Store( xorstr( "Paleto Bay" ) );
			if ( strcmp( key, xorstr( "Central Bank" ) ) == 0 ) return Translate_Store( xorstr( "Central Bank" ) );
			if ( strcmp( key, xorstr( "Cassino" ) ) == 0 ) return Translate_Store( xorstr( "Casino" ) );
			if ( strcmp( key, xorstr( "Los Santos Airport" ) ) == 0 ) return Translate_Store( xorstr( "Los Santos Airport" ) );
			if ( strcmp( key, xorstr( "Locations" ) ) == 0 ) return Translate_Store( xorstr( "Locations" ) );
			if ( strcmp( key, xorstr( "Select a location to teleport" ) ) == 0 ) return Translate_Store( xorstr( "Select a location to teleport" ) );
			if ( strcmp( key, xorstr( "Teleport to" ) ) == 0 ) return Translate_Store( xorstr( "Teleport to" ) );
			if ( strcmp( key, xorstr( "Custom Location" ) ) == 0 ) return Translate_Store( xorstr( "Custom Location" ) );
			if ( strcmp( key, xorstr( "Save Current Position" ) ) == 0 ) return Translate_Store( xorstr( "Save Current Position" ) );
			if ( strcmp( key, xorstr( "Delete Selected Location" ) ) == 0 ) return Translate_Store( xorstr( "Delete Selected Location" ) );
			if ( strcmp( key, xorstr( "Recoil Control" ) ) == 0 ) return Translate_Store( xorstr( "Recoil Control" ) );
			if ( strcmp( key, xorstr( "Value" ) ) == 0 ) return Translate_Store( xorstr( "Value" ) );
			if ( strcmp( key, xorstr( "Spread Control" ) ) == 0 ) return Translate_Store( xorstr( "Spread Control" ) );
			if ( strcmp( key, xorstr( "Range Control" ) ) == 0 ) return Translate_Store( xorstr( "Range Control" ) );
			if ( strcmp( key, xorstr( "Damage Control" ) ) == 0 ) return Translate_Store( xorstr( "Damage Control" ) );
			if ( strcmp( key, xorstr( "Size Control" ) ) == 0 ) return Translate_Store( xorstr( "Size Control" ) );
			if ( strcmp( key, xorstr( "Velocity Control" ) ) == 0 ) return Translate_Store( xorstr( "Velocity Control" ) );
			if ( strcmp( key, xorstr( "Rapid Fire" ) ) == 0 ) return Translate_Store( xorstr( "Rapid Fire" ) );
			if ( strcmp( key, xorstr( "Infinite Ammo" ) ) == 0 ) return Translate_Store( xorstr( "Infinite Ammo" ) );
			if ( strcmp( key, xorstr( "Ammo Value" ) ) == 0 ) return Translate_Store( xorstr( "Ammo Value" ) );
			if ( strcmp( key, xorstr( "Add Ammo" ) ) == 0 ) return Translate_Store( xorstr( "Add Ammo" ) );
			if ( strcmp( key, xorstr( "Get a weapon first" ) ) == 0 ) return Translate_Store( xorstr( "Get a weapon first" ) );
			if ( strcmp( key, xorstr( "Acceleration" ) ) == 0 ) return Translate_Store( xorstr( "Acceleration" ) );
			if ( strcmp( key, xorstr( "Break Force" ) ) == 0 ) return Translate_Store( xorstr( "Break Force" ) );
			if ( strcmp( key, xorstr( "Traction Curve Min" ) ) == 0 ) return Translate_Store( xorstr( "Traction Curve Min" ) );
			if ( strcmp( key, xorstr( "Weapon Spawn" ) ) == 0 ) return Translate_Store( xorstr( "Weapon Spawn" ) );
			if ( strcmp( key, xorstr( "Here you can spawn weapon for you" ) ) == 0 ) return Translate_Store( xorstr( "Here you can spawn weapon for you" ) );
			if ( strcmp( key, xorstr( "Select Player" ) ) == 0 ) return Translate_Store( xorstr( "Select Player" ) );
			if ( strcmp( key, xorstr( "Name" ) ) == 0 ) return Translate_Store( xorstr( "Name" ) );
			if ( strcmp( key, xorstr( "Id" ) ) == 0 ) return Translate_Store( xorstr( "ID" ) );
			if ( strcmp( key, xorstr( "Weapon" ) ) == 0 ) return Translate_Store( xorstr( "Weapon" ) );
			if ( strcmp( key, xorstr( "Friend" ) ) == 0 ) return Translate_Store( xorstr( "Friend" ) );
			if ( strcmp( key, xorstr( "True" ) ) == 0 ) return Translate_Store( xorstr( "True" ) );
			if ( strcmp( key, xorstr( "False" ) ) == 0 ) return Translate_Store( xorstr( "False" ) );
			if ( strcmp( key, xorstr( "Info" ) ) == 0 ) return Translate_Store( xorstr( "Info" ) );
			if ( strcmp( key, xorstr( "Teleport" ) ) == 0 ) return Translate_Store( xorstr( "Teleport" ) );
			if ( strcmp( key, xorstr( "Select Vehicle" ) ) == 0 ) return Translate_Store( xorstr( "Select Vehicle" ) );
			if ( strcmp( key, xorstr( "Driver" ) ) == 0 ) return Translate_Store( xorstr( "Driver" ) );
			if ( strcmp( key, xorstr( "Unknown" ) ) == 0 ) return Translate_Store( xorstr( "Unknown" ) );
			if ( strcmp( key, xorstr( "NPC" ) ) == 0 ) return Translate_Store( xorstr( "NPC" ) );
			if ( strcmp( key, xorstr( "Yes" ) ) == 0 ) return Translate_Store( xorstr( "Yes" ) );
			if ( strcmp( key, xorstr( "No" ) ) == 0 ) return Translate_Store( xorstr( "No" ) );
			if ( strcmp( key, xorstr( "Unlock" ) ) == 0 ) return Translate_Store( xorstr( "Unlock" ) );
			if ( strcmp( key, xorstr( "Lock" ) ) == 0 ) return Translate_Store( xorstr( "Lock" ) );
			if ( strcmp( key, xorstr( "Melee" ) ) == 0 ) return Translate_Store( xorstr( "Melee" ) );
			if ( strcmp( key, xorstr( "Handguns" ) ) == 0 ) return Translate_Store( xorstr( "Handguns" ) );
			if ( strcmp( key, xorstr( "SMGs" ) ) == 0 ) return Translate_Store( xorstr( "SMGs" ) );
			if ( strcmp( key, xorstr( "Shotguns" ) ) == 0 ) return Translate_Store( xorstr( "Shotguns" ) );
			if ( strcmp( key, xorstr( "Assault Rifles" ) ) == 0 ) return Translate_Store( xorstr( "Assault Rifles" ) );
			if ( strcmp( key, xorstr( "Machine Guns" ) ) == 0 ) return Translate_Store( xorstr( "Machine Guns" ) );
			if ( strcmp( key, xorstr( "Sniper Rifles" ) ) == 0 ) return Translate_Store( xorstr( "Sniper Rifles" ) );
			if ( strcmp( key, xorstr( "Heavy Weapons" ) ) == 0 ) return Translate_Store( xorstr( "Heavy Weapons" ) );
			if ( strcmp( key, xorstr( "Throwables" ) ) == 0 ) return Translate_Store( xorstr( "Throwables" ) );
			if ( strcmp( key, xorstr( "Misc" ) ) == 0 ) return Translate_Store( xorstr( "Misc" ) );
			if ( strcmp( key, xorstr( "has been spawned" ) ) == 0 ) return Translate_Store( xorstr( "has been spawned" ) );
			if ( strcmp( key, xorstr( "Resources" ) ) == 0 ) return Translate_Store( xorstr( "Resources" ) );
			if ( strcmp( key, xorstr( "List of current server resources" ) ) == 0 ) return Translate_Store( xorstr( "List of current server resources" ) );
			if ( strcmp( key, xorstr( "has been destroyed" ) ) == 0 ) return Translate_Store( xorstr( "has been destroyed" ) );
			if ( strcmp( key, xorstr( "Resource Stop is currently unavailable" ) ) == 0 ) return Translate_Store( xorstr( "Resource Stop is currently unavailable" ) );
			if ( strcmp( key, xorstr( "Lua Executor" ) ) == 0 ) return Translate_Store( xorstr( "Lua Executor" ) );
			if ( strcmp( key, xorstr( "Write and execute Lua code in the selected resource" ) ) == 0 ) return Translate_Store( xorstr( "Write and execute Lua code in the selected resource" ) );
			if ( strcmp( key, xorstr( "Select Resource" ) ) == 0 ) return Translate_Store( xorstr( "Select Resource" ) );
			if ( strcmp( key, xorstr( "No resources available" ) ) == 0 ) return Translate_Store( xorstr( "No resources available" ) );
			if ( strcmp( key, xorstr( "Execute" ) ) == 0 ) return Translate_Store( xorstr( "Execute" ) );
			if ( strcmp( key, xorstr( "Invalid resource selected" ) ) == 0 ) return Translate_Store( xorstr( "Invalid resource selected" ) );
			if ( strcmp( key, xorstr( "Code executed successfully" ) ) == 0 ) return Translate_Store( xorstr( "Code executed successfully" ) );
			if ( strcmp( key, xorstr( "Toggle" ) ) == 0 ) return Translate_Store( xorstr( "Toggle" ) );
			if ( strcmp( key, xorstr( "Player Names" ) ) == 0 ) return Translate_Store( xorstr( "Player Names" ) );
			if ( strcmp( key, xorstr( "Head Circle" ) ) == 0 ) return Translate_Store( xorstr( "Head Circle" ) );
			if ( strcmp( key, xorstr( "Health Bar" ) ) == 0 ) return Translate_Store( xorstr( "Health Bar" ) );
			if ( strcmp( key, xorstr( "Armor Bar" ) ) == 0 ) return Translate_Store( xorstr( "Armour Bar" ) );
			if ( strcmp( key, xorstr( "Highlight Visible" ) ) == 0 ) return Translate_Store( xorstr( "Highlight Visible" ) );
			if ( strcmp( key, xorstr( "Friend Marker" ) ) == 0 ) return Translate_Store( xorstr( "Friend Marker" ) );
			if ( strcmp( key, xorstr( "Show LocalPlayer" ) ) == 0 ) return Translate_Store( xorstr( "Show LocalPlayer" ) );
			if ( strcmp( key, xorstr( "Ignore Dead" ) ) == 0 ) return Translate_Store( xorstr( "Ignore Dead" ) );
			if ( strcmp( key, xorstr( "Vehicle Names" ) ) == 0 ) return Translate_Store( xorstr( "Vehicle Names" ) );
			if ( strcmp( key, xorstr( "Locked/Unlocked" ) ) == 0 ) return Translate_Store( xorstr( "Locked/Unlocked" ) );
			if ( strcmp( key, xorstr( "Stream Mode enabled" ) ) == 0 ) return Translate_Store( xorstr( "Stream Mode enabled" ) );
			if ( strcmp( key, xorstr( "Stream Mode disabled" ) ) == 0 ) return Translate_Store( xorstr( "Stream Mode disabled" ) );
			if ( strcmp( key, xorstr( "Failed to apply Stream Mode" ) ) == 0 ) return Translate_Store( xorstr( "Failed to apply Stream Mode" ) );
			if ( strcmp( key, xorstr( "Failed to revert Stream Mode" ) ) == 0 ) return Translate_Store( xorstr( "Failed to revert Stream Mode" ) );
			if ( strcmp( key, xorstr( "Configs" ) ) == 0 ) return Translate_Store( xorstr( "Configurations" ) );
			if ( strcmp( key, xorstr( "Hides this window from screen/stream capture" ) ) == 0 ) return Translate_Store( xorstr( "Hides this window from screen/stream capture" ) );
			if ( strcmp( key, xorstr( "That function can be detected by server AntiCheat" ) ) == 0 ) return Translate_Store( xorstr( "That function can be detected by server AntiCheat" ) );
			if ( strcmp( key, xorstr( "You must be in a car to use this function" ) ) == 0 ) return Translate_Store( xorstr( "You must be in a car to use this function" ) );
			if ( strcmp( key, xorstr( "System Client Alert" ) ) == 0 ) return Translate_Store( xorstr( "System Client Alert" ) );
			if ( strcmp( key, xorstr( "Portugues (Brasil)" ) ) == 0 ) return Translate_Store( xorstr( "Portuguese (Brazil)" ) );
			if ( strcmp( key, xorstr( "English (UK)" ) ) == 0 ) return Translate_Store( xorstr( "English (UK)" ) );
			if ( strcmp( key, xorstr( "English (US)" ) ) == 0 ) return Translate_Store( xorstr( "English (US)" ) );
			if ( strcmp( key, xorstr( "Russkii" ) ) == 0 ) return Translate_Store( xorstr( "Russian" ) );
			if ( strcmp( key, xorstr( "Spanish" ) ) == 0 ) return Translate_Store( xorstr( "Spanish" ) );
			break;
		}
		case Language::EnglishUS:
		{
			#define EN( Portuguese, English ) if ( strcmp( key, xorstr( Portuguese ) ) == 0 ) return Translate_Store( xorstr( English ) )

			EN( "Voltar", "Back" );
			EN( "Aceleração", "Acceleration" );
			EN( "Força de freio", "Brake force" );
			EN( "Aderência mínima", "Minimum traction" );
			EN( "Cassino", "Casino" );
			EN( "Liga o aimbot. Ele só age com arma na mão e mirando.", "Enables aiming assistance while you are aiming with a weapon." );
			EN( "O aimbot não vai mudar de alvo. Você vai precisar soltar e apertar a tecla de novo para trocar.", "Keeps the current target until the aim key is released." );
			EN( "Desenha na tela o círculo do campo de mira do aimbot.", "Draws the aimbot field-of-view circle." );
			EN( "Ignora quem está atrás de parede.", "Ignores targets hidden behind walls." );
			EN( "Mira só em jogadores de verdade, ignora os NPCs.", "Targets players only and ignores NPCs." );
			EN( "Liga o triggerbot: atira sozinho quando o alvo entra na mira.", "Fires automatically when a target enters your aim." );
			EN( "Liga o silent aim: o tiro acerta o alvo sem a câmera se mexer.", "Redirects shots to the target without moving the camera." );
			EN( "Desenha na tela o círculo do campo do silent aim.", "Draws the SilentAim field-of-view circle." );
			EN( "Redireciona o tiro para o alvo sem mover a câmera.", "Redirects shots to the target without moving the camera." );

			EN( "Liga o ESP de jogadores.", "Enables player ESP." );
			EN( "Desenha uma caixa em volta do jogador.", "Draws a box around the player." );
			EN( "Preenche a caixa com uma cor translúcida.", "Fills the box with a translucent color." );
			EN( "Mostra o nome do jogador acima da caixa.", "Shows the player name above the box." );
			EN( "Mostra qual arma o jogador está segurando.", "Shows the weapon currently held by the player." );
			EN( "Mostra a distância até o jogador, em metros.", "Shows the distance to the player in meters." );
			EN( "Desenha o esqueleto do jogador.", "Draws the player's skeleton." );
			EN( "Desenha um círculo na cabeça do jogador.", "Draws a circle around the player's head." );
			EN( "Barra de vida ao lado da caixa.", "Shows a health bar beside the box." );
			EN( "Barra de colete ao lado da caixa.", "Shows an armor bar beside the box." );
			EN( "Desenha uma linha da sua mira até o jogador.", "Draws a line from your crosshair to the player." );
			EN( "Desenha o ESP no seu próprio personagem.", "Draws ESP on your own character." );
			EN( "Não desenha jogadores mortos.", "Does not draw dead players." );
			EN( "Não desenha NPCs, só jogadores de verdade.", "Draws real players only and ignores NPCs." );
			EN( "Liga o ESP de veículos.", "Enables vehicle ESP." );
			EN( "Mostra o nome do modelo do veículo.", "Shows the vehicle model name." );
			EN( "Mostra se o veículo está trancado ou aberto.", "Shows whether the vehicle is locked or unlocked." );
			EN( "Desenha uma linha da sua mira até o veículo.", "Draws a line from your crosshair to the vehicle." );
			EN( "Mostra a distância até o veículo, em metros.", "Shows the distance to the vehicle in meters." );

			EN( "Você para de receber dano.", "Prevents your character from taking damage." );
			EN( "Pressione a tecla perto de um veículo para levantar, pressione de novo para arremessar.", "Press the key near a vehicle to lift it, then press it again to throw it." );
			EN( "Impede que os outros jogadores travem a mira em você.", "Prevents other players from locking their aim onto you." );
			EN( "Voa e atravessa paredes. Use a tecla escolhida para ligar e desligar.", "Fly through walls and toggle it with the selected key." );
			EN( "Aumenta a sua velocidade de corrida.", "Increases your running speed." );
			EN( "Tiro na cabeça deixa de ser fatal.", "Prevents headshots from being fatal." );
			EN( "Encolhe o seu personagem, ficando mais difícil de acertar.", "Shrinks your character, making it harder to hit." );
			EN( "Seu personagem não cai mais no chão ao levar impacto.", "Prevents your character from ragdolling on impact." );
			EN( "Puxa o motorista para fora ao entrar no veículo.", "Pulls the driver out when you enter a vehicle." );
			EN( "Rola sem limite, sem esperar entre um rolamento e outro.", "Removes the delay between combat rolls." );
			EN( "Corre sem cansar.", "Run without losing stamina." );
			EN( "Mantém a roda de armas aberta.", "Keeps the weapon wheel open." );
			EN( "Deixa o seu personagem invisível para os outros.", "Makes your character invisible to other players." );
			EN( "O veículo para de receber dano.", "Prevents the vehicle from taking damage." );
			EN( "Tranca ou destranca as portas do veículo.", "Locks or unlocks the vehicle doors." );
			EN( "Você não é arremessado para fora do veículo na batida.", "Prevents you from being thrown out of the vehicle in a crash." );
			EN( "Abre a página de edição do handling do veículo.", "Opens the vehicle handling editor." );
			EN( "Recarrega o boost do veículo usando a native do jogo.", "Refills the vehicle rocket boost." );
			EN( "Remove ou reduz o coice da arma.", "Removes or reduces weapon recoil." );
			EN( "Remove ou reduz a dispersão do tiro.", "Removes or reduces weapon spread." );
			EN( "Altera o alcance da arma.", "Changes the weapon range." );
			EN( "Altera o dano da arma.", "Changes the weapon damage." );
			EN( "Altera o tamanho do projétil.", "Changes the projectile size." );
			EN( "Altera a velocidade do projétil.", "Changes the projectile velocity." );
			EN( "Aumenta a cadência de tiro da arma.", "Increases the weapon fire rate." );
			EN( "Munição infinita, sem precisar recarregar.", "Provides infinite ammunition without reloading." );

			EN( "Marca esse jogador como amigo. Amigos são ignorados pelo aimbot e pelo ESP de inimigos.", "Marks this player as a friend. Friends are ignored by aimbot and enemy ESP." );
			EN( "Sincroniza o menu com a taxa de atualização do monitor.", "Synchronizes the menu with the monitor refresh rate." );
			EN( "Esconde o menu de programas de gravação e transmissão.", "Hides the menu from recording and streaming software." );
			EN( "Spawnar arma pode ser detectado pelo AntiCheat do servidor", "Spawning a weapon can be detected by the server AntiCheat" );

			#undef EN
			if ( strcmp( key, xorstr( "Combat" ) ) == 0 ) return Translate_Store( xorstr( "Combat" ) );
			if ( strcmp( key, xorstr( "Visuals" ) ) == 0 ) return Translate_Store( xorstr( "Visuals" ) );
			if ( strcmp( key, xorstr( "Local" ) ) == 0 ) return Translate_Store( xorstr( "Local" ) );
			if ( strcmp( key, xorstr( "World" ) ) == 0 ) return Translate_Store( xorstr( "World" ) );
			if ( strcmp( key, xorstr( "Exploits" ) ) == 0 ) return Translate_Store( xorstr( "Exploits" ) );
			if ( strcmp( key, xorstr( "Settings" ) ) == 0 ) return Translate_Store( xorstr( "Settings" ) );
			if ( strcmp( key, xorstr( "General" ) ) == 0 ) return Translate_Store( xorstr( "General" ) );
			if ( strcmp( key, xorstr( "Aimbot" ) ) == 0 ) return Translate_Store( xorstr( "Aimbot" ) );
			if ( strcmp( key, xorstr( "TriggerBot" ) ) == 0 ) return Translate_Store( xorstr( "TriggerBot" ) );
			if ( strcmp( key, xorstr( "SilentAim" ) ) == 0 ) return Translate_Store( xorstr( "SilentAim" ) );
			if ( strcmp( key, xorstr( "Players" ) ) == 0 ) return Translate_Store( xorstr( "Players" ) );
			if ( strcmp( key, xorstr( "Vehicles" ) ) == 0 ) return Translate_Store( xorstr( "Vehicles" ) );
			if ( strcmp( key, xorstr( "Main" ) ) == 0 ) return Translate_Store( xorstr( "Main" ) );
			if ( strcmp( key, xorstr( "Teleports" ) ) == 0 ) return Translate_Store( xorstr( "Teleports" ) );
			if ( strcmp( key, xorstr( "Weapons" ) ) == 0 ) return Translate_Store( xorstr( "Weapons" ) );
			if ( strcmp( key, xorstr( "Globals" ) ) == 0 ) return Translate_Store( xorstr( "Enable" ) );
			if ( strcmp( key, xorstr( "Bind" ) ) == 0 ) return Translate_Store( xorstr( "Bind" ) );
			if ( strcmp( key, xorstr( "Show Fov" ) ) == 0 ) return Translate_Store( xorstr( "Show FOV" ) );
			if ( strcmp( key, xorstr( "Only Visible" ) ) == 0 ) return Translate_Store( xorstr( "Only Visible" ) );
			if ( strcmp( key, xorstr( "Ignore NPCs" ) ) == 0 ) return Translate_Store( xorstr( "Ignore NPCs" ) );
			if ( strcmp( key, xorstr( "Globals Vector" ) ) == 0 ) return Translate_Store( xorstr( "Settings" ) );
			if ( strcmp( key, xorstr( "Field of View" ) ) == 0 ) return Translate_Store( xorstr( "Field of View" ) );
			if ( strcmp( key, xorstr( "Smooth" ) ) == 0 ) return Translate_Store( xorstr( "Smooth" ) );
			if ( strcmp( key, xorstr( "Max Distance" ) ) == 0 ) return Translate_Store( xorstr( "Max Distance" ) );
			if ( strcmp( key, xorstr( "Smart Triggerbot" ) ) == 0 ) return Translate_Store( xorstr( "Smart Triggerbot" ) );
			if ( strcmp( key, xorstr( "Delay" ) ) == 0 ) return Translate_Store( xorstr( "Delay" ) );
			if ( strcmp( key, xorstr( "Magic Bullets" ) ) == 0 ) return Translate_Store( xorstr( "Magic Bullets" ) );
			if ( strcmp( key, xorstr( "Miss Chance" ) ) == 0 ) return Translate_Store( xorstr( "Miss Chance" ) );
			if ( strcmp( key, xorstr( "VSync" ) ) == 0 ) return Translate_Store( xorstr( "VSync" ) );
			if ( strcmp( key, xorstr( "Stream Proof" ) ) == 0 ) return Translate_Store( xorstr( "Stream Proof" ) );
			if ( strcmp( key, xorstr( "Stream Mode" ) ) == 0 ) return Translate_Store( xorstr( "Stream Mode" ) );
			if ( strcmp( key, xorstr( "Menu Key" ) ) == 0 ) return Translate_Store( xorstr( "Menu Key" ) );
			if ( strcmp( key, xorstr( "Unload" ) ) == 0 ) return Translate_Store( xorstr( "Unload" ) );
			if ( strcmp( key, xorstr( "Config Vector" ) ) == 0 ) return Translate_Store( xorstr( "Configurations" ) );
			if ( strcmp( key, xorstr( "Export Config" ) ) == 0 ) return Translate_Store( xorstr( "Export Config" ) );
			if ( strcmp( key, xorstr( "Import Config" ) ) == 0 ) return Translate_Store( xorstr( "Import Config" ) );
			if ( strcmp( key, xorstr( "Colors" ) ) == 0 ) return Translate_Store( xorstr( "Colors" ) );
			if ( strcmp( key, xorstr( "Aimbot Fov" ) ) == 0 ) return Translate_Store( xorstr( "Aimbot FOV" ) );
			if ( strcmp( key, xorstr( "Triggerbot Fov" ) ) == 0 ) return Translate_Store( xorstr( "Triggerbot FOV" ) );
			if ( strcmp( key, xorstr( "SilentAim Fov" ) ) == 0 ) return Translate_Store( xorstr( "SilentAim FOV" ) );
			if ( strcmp( key, xorstr( "Box" ) ) == 0 ) return Translate_Store( xorstr( "Box" ) );
			if ( strcmp( key, xorstr( "Filled Box" ) ) == 0 ) return Translate_Store( xorstr( "Filled Box" ) );
			if ( strcmp( key, xorstr( "Skeleton" ) ) == 0 ) return Translate_Store( xorstr( "Skeleton" ) );
			if ( strcmp( key, xorstr( "SnapLines" ) ) == 0 ) return Translate_Store( xorstr( "SnapLines" ) );
			if ( strcmp( key, xorstr( "Names" ) ) == 0 ) return Translate_Store( xorstr( "Names" ) );
			if ( strcmp( key, xorstr( "Weapon Names" ) ) == 0 ) return Translate_Store( xorstr( "Weapon Names" ) );
			if ( strcmp( key, xorstr( "Distance" ) ) == 0 ) return Translate_Store( xorstr( "Distance" ) );
			if ( strcmp( key, xorstr( "GodMode" ) ) == 0 ) return Translate_Store( xorstr( "God Mode" ) );
			if ( strcmp( key, xorstr( "Block Aiming" ) ) == 0 ) return Translate_Store( xorstr( "Block Aiming" ) );
			if ( strcmp( key, xorstr( "NoClip" ) ) == 0 ) return Translate_Store( xorstr( "Noclip" ) );
			if ( strcmp( key, xorstr( "Speed" ) ) == 0 ) return Translate_Store( xorstr( "Speed" ) );
			if ( strcmp( key, xorstr( "Fast Run" ) ) == 0 ) return Translate_Store( xorstr( "Fast Run" ) );
			if ( strcmp( key, xorstr( "Anti Headshot" ) ) == 0 ) return Translate_Store( xorstr( "Anti Headshot" ) );
			if ( strcmp( key, xorstr( "Shrink" ) ) == 0 ) return Translate_Store( xorstr( "Shrink" ) );
			if ( strcmp( key, xorstr( "No RagDoll" ) ) == 0 ) return Translate_Store( xorstr( "No RagDoll" ) );
			if ( strcmp( key, xorstr( "Steal Car" ) ) == 0 ) return Translate_Store( xorstr( "Steal Car" ) );
			if ( strcmp( key, xorstr( "Infinite CombatRoll" ) ) == 0 ) return Translate_Store( xorstr( "Infinite Combat Roll" ) );
			if ( strcmp( key, xorstr( "Infinite Stamina" ) ) == 0 ) return Translate_Store( xorstr( "Infinite Stamina" ) );
			if ( strcmp( key, xorstr( "Force Tab" ) ) == 0 ) return Translate_Store( xorstr( "Force Tab" ) );
			if ( strcmp( key, xorstr( "Invisible" ) ) == 0 ) return Translate_Store( xorstr( "Invisible" ) );
			if ( strcmp( key, xorstr( "Health" ) ) == 0 ) return Translate_Store( xorstr( "Health" ) );
			if ( strcmp( key, xorstr( "Armor" ) ) == 0 ) return Translate_Store( xorstr( "Armor" ) );
			if ( strcmp( key, xorstr( "Doors Locked" ) ) == 0 ) return Translate_Store( xorstr( "Doors Locked" ) );
			if ( strcmp( key, xorstr( "Don't Fall" ) ) == 0 ) return Translate_Store( xorstr( "Don't Fall" ) );
			if ( strcmp( key, xorstr( "Handling Editor" ) ) == 0 ) return Translate_Store( xorstr( "Handling Editor" ) );
			if ( strcmp( key, xorstr( "Actions" ) ) == 0 ) return Translate_Store( xorstr( "Actions" ) );
			if ( strcmp( key, xorstr( "Repair Vehicle" ) ) == 0 ) return Translate_Store( xorstr( "Repair Vehicle" ) );
			if ( strcmp( key, xorstr( "Bind Fix" ) ) == 0 ) return Translate_Store( xorstr( "Bind Fix" ) );
			if ( strcmp( key, xorstr( "Bind Unlock" ) ) == 0 ) return Translate_Store( xorstr( "Bind Unlock" ) );
			if ( strcmp( key, xorstr( "Waypoint" ) ) == 0 ) return Translate_Store( xorstr( "Waypoint" ) );
			if ( strcmp( key, xorstr( "Square" ) ) == 0 ) return Translate_Store( xorstr( "Plaza" ) );
			if ( strcmp( key, xorstr( "Pier" ) ) == 0 ) return Translate_Store( xorstr( "Pier" ) );
			if ( strcmp( key, xorstr( "Paleto Bay" ) ) == 0 ) return Translate_Store( xorstr( "Paleto Bay" ) );
			if ( strcmp( key, xorstr( "Central Bank" ) ) == 0 ) return Translate_Store( xorstr( "Central Bank" ) );
			if ( strcmp( key, xorstr( "Cassino" ) ) == 0 ) return Translate_Store( xorstr( "Casino" ) );
			if ( strcmp( key, xorstr( "Los Santos Airport" ) ) == 0 ) return Translate_Store( xorstr( "Los Santos Airport" ) );
			if ( strcmp( key, xorstr( "Locations" ) ) == 0 ) return Translate_Store( xorstr( "Locations" ) );
			if ( strcmp( key, xorstr( "Select a location to teleport" ) ) == 0 ) return Translate_Store( xorstr( "Select a location to teleport" ) );
			if ( strcmp( key, xorstr( "Teleport to" ) ) == 0 ) return Translate_Store( xorstr( "Teleport to" ) );
			if ( strcmp( key, xorstr( "Custom Location" ) ) == 0 ) return Translate_Store( xorstr( "Custom Location" ) );
			if ( strcmp( key, xorstr( "Save Current Position" ) ) == 0 ) return Translate_Store( xorstr( "Save Current Position" ) );
			if ( strcmp( key, xorstr( "Delete Selected Location" ) ) == 0 ) return Translate_Store( xorstr( "Delete Selected Location" ) );
			if ( strcmp( key, xorstr( "Recoil Control" ) ) == 0 ) return Translate_Store( xorstr( "Recoil Control" ) );
			if ( strcmp( key, xorstr( "Value" ) ) == 0 ) return Translate_Store( xorstr( "Value" ) );
			if ( strcmp( key, xorstr( "Spread Control" ) ) == 0 ) return Translate_Store( xorstr( "Spread Control" ) );
			if ( strcmp( key, xorstr( "Range Control" ) ) == 0 ) return Translate_Store( xorstr( "Range Control" ) );
			if ( strcmp( key, xorstr( "Damage Control" ) ) == 0 ) return Translate_Store( xorstr( "Damage Control" ) );
			if ( strcmp( key, xorstr( "Size Control" ) ) == 0 ) return Translate_Store( xorstr( "Size Control" ) );
			if ( strcmp( key, xorstr( "Velocity Control" ) ) == 0 ) return Translate_Store( xorstr( "Velocity Control" ) );
			if ( strcmp( key, xorstr( "Rapid Fire" ) ) == 0 ) return Translate_Store( xorstr( "Rapid Fire" ) );
			if ( strcmp( key, xorstr( "Infinite Ammo" ) ) == 0 ) return Translate_Store( xorstr( "Infinite Ammo" ) );
			if ( strcmp( key, xorstr( "Ammo Value" ) ) == 0 ) return Translate_Store( xorstr( "Ammo Value" ) );
			if ( strcmp( key, xorstr( "Add Ammo" ) ) == 0 ) return Translate_Store( xorstr( "Add Ammo" ) );
			if ( strcmp( key, xorstr( "Get a weapon first" ) ) == 0 ) return Translate_Store( xorstr( "Get a weapon first" ) );
			if ( strcmp( key, xorstr( "Acceleration" ) ) == 0 ) return Translate_Store( xorstr( "Acceleration" ) );
			if ( strcmp( key, xorstr( "Break Force" ) ) == 0 ) return Translate_Store( xorstr( "Break Force" ) );
			if ( strcmp( key, xorstr( "Traction Curve Min" ) ) == 0 ) return Translate_Store( xorstr( "Traction Curve Min" ) );
			if ( strcmp( key, xorstr( "Weapon Spawn" ) ) == 0 ) return Translate_Store( xorstr( "Weapon Spawn" ) );
			if ( strcmp( key, xorstr( "Here you can spawn weapon for you" ) ) == 0 ) return Translate_Store( xorstr( "Here you can spawn weapon for you" ) );
			if ( strcmp( key, xorstr( "Select Player" ) ) == 0 ) return Translate_Store( xorstr( "Select Player" ) );
			if ( strcmp( key, xorstr( "Name" ) ) == 0 ) return Translate_Store( xorstr( "Name" ) );
			if ( strcmp( key, xorstr( "Id" ) ) == 0 ) return Translate_Store( xorstr( "ID" ) );
			if ( strcmp( key, xorstr( "Weapon" ) ) == 0 ) return Translate_Store( xorstr( "Weapon" ) );
			if ( strcmp( key, xorstr( "Friend" ) ) == 0 ) return Translate_Store( xorstr( "Friend" ) );
			if ( strcmp( key, xorstr( "True" ) ) == 0 ) return Translate_Store( xorstr( "True" ) );
			if ( strcmp( key, xorstr( "False" ) ) == 0 ) return Translate_Store( xorstr( "False" ) );
			if ( strcmp( key, xorstr( "Info" ) ) == 0 ) return Translate_Store( xorstr( "Info" ) );
			if ( strcmp( key, xorstr( "Teleport" ) ) == 0 ) return Translate_Store( xorstr( "Teleport" ) );
			if ( strcmp( key, xorstr( "Select Vehicle" ) ) == 0 ) return Translate_Store( xorstr( "Select Vehicle" ) );
			if ( strcmp( key, xorstr( "Driver" ) ) == 0 ) return Translate_Store( xorstr( "Driver" ) );
			if ( strcmp( key, xorstr( "Unknown" ) ) == 0 ) return Translate_Store( xorstr( "Unknown" ) );
			if ( strcmp( key, xorstr( "NPC" ) ) == 0 ) return Translate_Store( xorstr( "NPC" ) );
			if ( strcmp( key, xorstr( "Yes" ) ) == 0 ) return Translate_Store( xorstr( "Yes" ) );
			if ( strcmp( key, xorstr( "No" ) ) == 0 ) return Translate_Store( xorstr( "No" ) );
			if ( strcmp( key, xorstr( "Unlock" ) ) == 0 ) return Translate_Store( xorstr( "Unlock" ) );
			if ( strcmp( key, xorstr( "Lock" ) ) == 0 ) return Translate_Store( xorstr( "Lock" ) );
			if ( strcmp( key, xorstr( "Melee" ) ) == 0 ) return Translate_Store( xorstr( "Melee" ) );
			if ( strcmp( key, xorstr( "Handguns" ) ) == 0 ) return Translate_Store( xorstr( "Handguns" ) );
			if ( strcmp( key, xorstr( "SMGs" ) ) == 0 ) return Translate_Store( xorstr( "SMGs" ) );
			if ( strcmp( key, xorstr( "Shotguns" ) ) == 0 ) return Translate_Store( xorstr( "Shotguns" ) );
			if ( strcmp( key, xorstr( "Assault Rifles" ) ) == 0 ) return Translate_Store( xorstr( "Assault Rifles" ) );
			if ( strcmp( key, xorstr( "Machine Guns" ) ) == 0 ) return Translate_Store( xorstr( "Machine Guns" ) );
			if ( strcmp( key, xorstr( "Sniper Rifles" ) ) == 0 ) return Translate_Store( xorstr( "Sniper Rifles" ) );
			if ( strcmp( key, xorstr( "Heavy Weapons" ) ) == 0 ) return Translate_Store( xorstr( "Heavy Weapons" ) );
			if ( strcmp( key, xorstr( "Throwables" ) ) == 0 ) return Translate_Store( xorstr( "Throwables" ) );
			if ( strcmp( key, xorstr( "Misc" ) ) == 0 ) return Translate_Store( xorstr( "Misc" ) );
			if ( strcmp( key, xorstr( "has been spawned" ) ) == 0 ) return Translate_Store( xorstr( "has been spawned" ) );
			if ( strcmp( key, xorstr( "Resources" ) ) == 0 ) return Translate_Store( xorstr( "Resources" ) );
			if ( strcmp( key, xorstr( "List of current server resources" ) ) == 0 ) return Translate_Store( xorstr( "List of current server resources" ) );
			if ( strcmp( key, xorstr( "has been destroyed" ) ) == 0 ) return Translate_Store( xorstr( "has been destroyed" ) );
			if ( strcmp( key, xorstr( "Resource Stop is currently unavailable" ) ) == 0 ) return Translate_Store( xorstr( "Resource Stop is currently unavailable" ) );
			if ( strcmp( key, xorstr( "Lua Executor" ) ) == 0 ) return Translate_Store( xorstr( "Lua Executor" ) );
			if ( strcmp( key, xorstr( "Write and execute Lua code in the selected resource" ) ) == 0 ) return Translate_Store( xorstr( "Write and execute Lua code in the selected resource" ) );
			if ( strcmp( key, xorstr( "Select Resource" ) ) == 0 ) return Translate_Store( xorstr( "Select Resource" ) );
			if ( strcmp( key, xorstr( "No resources available" ) ) == 0 ) return Translate_Store( xorstr( "No resources available" ) );
			if ( strcmp( key, xorstr( "Execute" ) ) == 0 ) return Translate_Store( xorstr( "Execute" ) );
			if ( strcmp( key, xorstr( "Invalid resource selected" ) ) == 0 ) return Translate_Store( xorstr( "Invalid resource selected" ) );
			if ( strcmp( key, xorstr( "Code executed successfully" ) ) == 0 ) return Translate_Store( xorstr( "Code executed successfully" ) );
			if ( strcmp( key, xorstr( "Toggle" ) ) == 0 ) return Translate_Store( xorstr( "Toggle" ) );
			if ( strcmp( key, xorstr( "Player Names" ) ) == 0 ) return Translate_Store( xorstr( "Player Names" ) );
			if ( strcmp( key, xorstr( "Head Circle" ) ) == 0 ) return Translate_Store( xorstr( "Head Circle" ) );
			if ( strcmp( key, xorstr( "Health Bar" ) ) == 0 ) return Translate_Store( xorstr( "Health Bar" ) );
			if ( strcmp( key, xorstr( "Armor Bar" ) ) == 0 ) return Translate_Store( xorstr( "Armor Bar" ) );
			if ( strcmp( key, xorstr( "Highlight Visible" ) ) == 0 ) return Translate_Store( xorstr( "Highlight Visible" ) );
			if ( strcmp( key, xorstr( "Friend Marker" ) ) == 0 ) return Translate_Store( xorstr( "Friend Marker" ) );
			if ( strcmp( key, xorstr( "Show LocalPlayer" ) ) == 0 ) return Translate_Store( xorstr( "Show LocalPlayer" ) );
			if ( strcmp( key, xorstr( "Ignore Dead" ) ) == 0 ) return Translate_Store( xorstr( "Ignore Dead" ) );
			if ( strcmp( key, xorstr( "Vehicle Names" ) ) == 0 ) return Translate_Store( xorstr( "Vehicle Names" ) );
			if ( strcmp( key, xorstr( "Locked/Unlocked" ) ) == 0 ) return Translate_Store( xorstr( "Locked/Unlocked" ) );
			if ( strcmp( key, xorstr( "Stream Mode enabled" ) ) == 0 ) return Translate_Store( xorstr( "Stream Mode enabled" ) );
			if ( strcmp( key, xorstr( "Stream Mode disabled" ) ) == 0 ) return Translate_Store( xorstr( "Stream Mode disabled" ) );
			if ( strcmp( key, xorstr( "Failed to apply Stream Mode" ) ) == 0 ) return Translate_Store( xorstr( "Failed to apply Stream Mode" ) );
			if ( strcmp( key, xorstr( "Failed to revert Stream Mode" ) ) == 0 ) return Translate_Store( xorstr( "Failed to revert Stream Mode" ) );
			if ( strcmp( key, xorstr( "Configs" ) ) == 0 ) return Translate_Store( xorstr( "Configurations" ) );
			if ( strcmp( key, xorstr( "Hides this window from screen/stream capture" ) ) == 0 ) return Translate_Store( xorstr( "Hides this window from screen/stream capture" ) );
			if ( strcmp( key, xorstr( "That function can be detected by server AntiCheat" ) ) == 0 ) return Translate_Store( xorstr( "That function can be detected by server AntiCheat" ) );
			if ( strcmp( key, xorstr( "You must be in a car to use this function" ) ) == 0 ) return Translate_Store( xorstr( "You must be in a car to use this function" ) );
			if ( strcmp( key, xorstr( "System Client Alert" ) ) == 0 ) return Translate_Store( xorstr( "System Client Alert" ) );
			if ( strcmp( key, xorstr( "Portugues (Brasil)" ) ) == 0 ) return Translate_Store( xorstr( "Portuguese (Brazil)" ) );
			if ( strcmp( key, xorstr( "English (UK)" ) ) == 0 ) return Translate_Store( xorstr( "English (UK)" ) );
			if ( strcmp( key, xorstr( "English (US)" ) ) == 0 ) return Translate_Store( xorstr( "English (US)" ) );
			if ( strcmp( key, xorstr( "Russkii" ) ) == 0 ) return Translate_Store( xorstr( "Russian" ) );
			if ( strcmp( key, xorstr( "Spanish" ) ) == 0 ) return Translate_Store( xorstr( "Spanish" ) );
			break;
		}
		case Language::Russian:
		{
			if (strcmp(key, xorstr("Combat")) == 0) return Translate_Store(xorstr("Boi"));
			if (strcmp(key, xorstr("Visuals")) == 0) return Translate_Store(xorstr("Vizualy"));
			if (strcmp(key, xorstr("Local")) == 0) return Translate_Store(xorstr("Lokalnyi"));
			if (strcmp(key, xorstr("World")) == 0) return Translate_Store(xorstr("Mir"));
			if (strcmp(key, xorstr("Exploits")) == 0) return Translate_Store(xorstr("Eksploity"));
			if (strcmp(key, xorstr("Settings")) == 0) return Translate_Store(xorstr("Nastroiki"));
			if (strcmp(key, xorstr("General")) == 0) return Translate_Store(xorstr("Obshchee"));
			if (strcmp(key, xorstr("Aimbot")) == 0) return Translate_Store(xorstr("Aimbot"));
			if (strcmp(key, xorstr("TriggerBot")) == 0) return Translate_Store(xorstr("Triggerbot"));
			if (strcmp(key, xorstr("SilentAim")) == 0) return Translate_Store(xorstr("Tikhii Pricel"));
			if (strcmp(key, xorstr("Players")) == 0) return Translate_Store(xorstr("Igroki"));
			if (strcmp(key, xorstr("Vehicles")) == 0) return Translate_Store(xorstr("Transport"));
			if (strcmp(key, xorstr("Main")) == 0) return Translate_Store(xorstr("Osnovnoe"));
			if (strcmp(key, xorstr("Teleports")) == 0) return Translate_Store(xorstr("Teleporty"));
			if (strcmp(key, xorstr("Weapons")) == 0) return Translate_Store(xorstr("Oruzhie"));
			if (strcmp(key, xorstr("Globals")) == 0) return Translate_Store(xorstr("Vklyuchit"));
			if (strcmp(key, xorstr("Bind")) == 0) return Translate_Store(xorstr("Klavisha"));
			if (strcmp(key, xorstr("Show Fov")) == 0) return Translate_Store(xorstr("Pokazat FOV"));
			if (strcmp(key, xorstr("Only Visible")) == 0) return Translate_Store(xorstr("Tolko Vidimye"));
			if (strcmp(key, xorstr("Ignore NPCs")) == 0) return Translate_Store(xorstr("Ignorirovat NPC"));
			if (strcmp(key, xorstr("Globals Vector")) == 0) return Translate_Store(xorstr("Nastroiki"));
			if (strcmp(key, xorstr("Field of View")) == 0) return Translate_Store(xorstr("Pole Zreniya"));
			if (strcmp(key, xorstr("Smooth")) == 0) return Translate_Store(xorstr("Sglazhivanie"));
			if (strcmp(key, xorstr("Max Distance")) == 0) return Translate_Store(xorstr("Maks Rasstoyanie"));
			if (strcmp(key, xorstr("Smart Triggerbot")) == 0) return Translate_Store(xorstr("Umnyi Triggerbot"));
			if (strcmp(key, xorstr("Delay")) == 0) return Translate_Store(xorstr("Zaderzhka"));
			if (strcmp(key, xorstr("Magic Bullets")) == 0) return Translate_Store(xorstr("Magicheskie Puli"));
			if (strcmp(key, xorstr("Miss Chance")) == 0) return Translate_Store(xorstr("Shans Promakha"));
			if (strcmp(key, xorstr("VSync")) == 0) return Translate_Store(xorstr("VSync"));
			if (strcmp(key, xorstr("Stream Proof")) == 0) return Translate_Store(xorstr("Zashchita Ot Strima"));
			if (strcmp(key, xorstr("Stream Mode")) == 0) return Translate_Store(xorstr("Rezhim Strima"));
			if (strcmp(key, xorstr("Menu Key")) == 0) return Translate_Store(xorstr("Klavisha Menyu"));
			if (strcmp(key, xorstr("Unload")) == 0) return Translate_Store(xorstr("Vygruzit"));
			if (strcmp(key, xorstr("Config Vector")) == 0) return Translate_Store(xorstr("Konfiguratsii"));
			if (strcmp(key, xorstr("Export Config")) == 0) return Translate_Store(xorstr("Eksport Konfiga"));
			if (strcmp(key, xorstr("Import Config")) == 0) return Translate_Store(xorstr("Import Konfiga"));
			if (strcmp(key, xorstr("Colors")) == 0) return Translate_Store(xorstr("Tsveta"));
			if (strcmp(key, xorstr("Aimbot Fov")) == 0) return Translate_Store(xorstr("FOV Aimbota"));
			if (strcmp(key, xorstr("Triggerbot Fov")) == 0) return Translate_Store(xorstr("FOV Triggerbota"));
			if (strcmp(key, xorstr("SilentAim Fov")) == 0) return Translate_Store(xorstr("FOV Tikhogo Pricela"));
			if (strcmp(key, xorstr("Box")) == 0) return Translate_Store(xorstr("Korobka"));
			if (strcmp(key, xorstr("Filled Box")) == 0) return Translate_Store(xorstr("Zapolnennaya Korobka"));
			if (strcmp(key, xorstr("Skeleton")) == 0) return Translate_Store(xorstr("Skelet"));
			if (strcmp(key, xorstr("SnapLines")) == 0) return Translate_Store(xorstr("Linii"));
			if (strcmp(key, xorstr("Names")) == 0) return Translate_Store(xorstr("Imena"));
			if (strcmp(key, xorstr("Weapon Names")) == 0) return Translate_Store(xorstr("Nazvaniya Oruzhiya"));
			if (strcmp(key, xorstr("Distance")) == 0) return Translate_Store(xorstr("Distantsiya"));
			if (strcmp(key, xorstr("GodMode")) == 0) return Translate_Store(xorstr("Rezhim Boga"));
			if (strcmp(key, xorstr("Block Aiming")) == 0) return Translate_Store(xorstr("Zablokirovat Pricel"));
			if (strcmp(key, xorstr("NoClip")) == 0) return Translate_Store(xorstr("Bez Kollizii"));
			if (strcmp(key, xorstr("Speed")) == 0) return Translate_Store(xorstr("Skorost"));
			if (strcmp(key, xorstr("Fast Run")) == 0) return Translate_Store(xorstr("Bystryi Beg"));
			if (strcmp(key, xorstr("Anti Headshot")) == 0) return Translate_Store(xorstr("Anti Vyistrel V Golovu"));
			if (strcmp(key, xorstr("Shrink")) == 0) return Translate_Store(xorstr("Umenshit"));
			if (strcmp(key, xorstr("No RagDoll")) == 0) return Translate_Store(xorstr("Bez Ragdoll"));
			if (strcmp(key, xorstr("Steal Car")) == 0) return Translate_Store(xorstr("Ukrast Mashinu"));
			if (strcmp(key, xorstr("Infinite CombatRoll")) == 0) return Translate_Store(xorstr("Beskonechnyi Rol"));
			if (strcmp(key, xorstr("Infinite Stamina")) == 0) return Translate_Store(xorstr("Beskonechnaya Vynoslivost"));
			if (strcmp(key, xorstr("Force Tab")) == 0) return Translate_Store(xorstr("Zastavit Vkladku"));
			if (strcmp(key, xorstr("Invisible")) == 0) return Translate_Store(xorstr("Nevidimyi"));
			if (strcmp(key, xorstr("Health")) == 0) return Translate_Store(xorstr("Zdorovie"));
			if (strcmp(key, xorstr("Armor")) == 0) return Translate_Store(xorstr("Broniya"));
			if (strcmp(key, xorstr("Doors Locked")) == 0) return Translate_Store(xorstr("Dveri Zablokirovany"));
			if (strcmp(key, xorstr("Don't Fall")) == 0) return Translate_Store(xorstr("Ne Padat"));
			if (strcmp(key, xorstr("Handling Editor")) == 0) return Translate_Store(xorstr("Redaktor Upravleniya"));
			if (strcmp(key, xorstr("Actions")) == 0) return Translate_Store(xorstr("Deistviya"));
			if (strcmp(key, xorstr("Repair Vehicle")) == 0) return Translate_Store(xorstr("Otremontirovat Transport"));
			if (strcmp(key, xorstr("Bind Fix")) == 0) return Translate_Store(xorstr("Ispravit Vyazku"));
			if (strcmp(key, xorstr("Bind Unlock")) == 0) return Translate_Store(xorstr("Razblokirovat Vyazku"));
			if (strcmp(key, xorstr("Waypoint")) == 0) return Translate_Store(xorstr("Tochka Marshrutom"));
			if (strcmp(key, xorstr("Square")) == 0) return Translate_Store(xorstr("Ploschad"));
			if (strcmp(key, xorstr("Pier")) == 0) return Translate_Store(xorstr("Prichal"));
			if (strcmp(key, xorstr("Paleto Bay")) == 0) return Translate_Store(xorstr("Paleto Bay"));
			if (strcmp(key, xorstr("Central Bank")) == 0) return Translate_Store(xorstr("Tsentralnyi Bank"));
			if (strcmp(key, xorstr("Cassino")) == 0) return Translate_Store(xorstr("Kazino"));
			if (strcmp(key, xorstr("Los Santos Airport")) == 0) return Translate_Store(xorstr("Aeroport Los Santos"));
			if (strcmp(key, xorstr("Locations")) == 0) return Translate_Store(xorstr("Lokatsii"));
			if (strcmp(key, xorstr("Select a location to teleport")) == 0) return Translate_Store(xorstr("Vyberite lokatsiyu dlya teleportatsii"));
			if (strcmp(key, xorstr("Teleport to")) == 0) return Translate_Store(xorstr("Teleportirovatsya v"));
			if (strcmp(key, xorstr("Custom Location")) == 0) return Translate_Store(xorstr("Personalizovannaya Lokatsiya"));
			if (strcmp(key, xorstr("Save Current Position")) == 0) return Translate_Store(xorstr("Sohranit Tekushchuyu Pozitsiyu"));
			if (strcmp(key, xorstr("Delete Selected Location")) == 0) return Translate_Store(xorstr("Udalit Vybrannuyu Lokatsiyu"));
			if (strcmp(key, xorstr("Recoil Control")) == 0) return Translate_Store(xorstr("Upravlenie Otdachei"));
			if (strcmp(key, xorstr("Value")) == 0) return Translate_Store(xorstr("Znachenie"));
			if (strcmp(key, xorstr("Spread Control")) == 0) return Translate_Store(xorstr("Upravlenie Rasseivaniem"));
			if (strcmp(key, xorstr("Range Control")) == 0) return Translate_Store(xorstr("Upravlenie Diastantsiei"));
			if (strcmp(key, xorstr("Damage Control")) == 0) return Translate_Store(xorstr("Upravlenie Uronom"));
			if (strcmp(key, xorstr("Size Control")) == 0) return Translate_Store(xorstr("Upravlenie Razmerom"));
			if (strcmp(key, xorstr("Velocity Control")) == 0) return Translate_Store(xorstr("Upravlenie Skorostyu"));
			if (strcmp(key, xorstr("Rapid Fire")) == 0) return Translate_Store(xorstr("Bystraya Strelba"));
			if (strcmp(key, xorstr("Infinite Ammo")) == 0) return Translate_Store(xorstr("Beskonechnye Patrony"));
			if (strcmp(key, xorstr("Ammo Value")) == 0) return Translate_Store(xorstr("Znachenie Patronov"));
			if (strcmp(key, xorstr("Add Ammo")) == 0) return Translate_Store(xorstr("Dobavit Patrony"));
			if (strcmp(key, xorstr("Get a weapon first")) == 0) return Translate_Store(xorstr("Snachala vozite oruzhie"));
			if (strcmp(key, xorstr("Acceleration")) == 0) return Translate_Store(xorstr("Uskorenie"));
			if (strcmp(key, xorstr("Break Force")) == 0) return Translate_Store(xorstr("Sila Tormozhenia"));
			if (strcmp(key, xorstr("Traction Curve Min")) == 0) return Translate_Store(xorstr("Min Krivaya Tyagi"));
			if (strcmp(key, xorstr("Weapon Spawn")) == 0) return Translate_Store(xorstr("Generatsiya Oruzhiya"));
			if (strcmp(key, xorstr("Here you can spawn weapon for you")) == 0) return Translate_Store(xorstr("Zdes vy mozete generirovat oruzhie dlya sebya"));
			if (strcmp(key, xorstr("Select Player")) == 0) return Translate_Store(xorstr("Vyberite Igroka"));
			if (strcmp(key, xorstr("Name")) == 0) return Translate_Store(xorstr("Imya"));
			if (strcmp(key, xorstr("Id")) == 0) return Translate_Store(xorstr("ID"));
			if (strcmp(key, xorstr("Weapon")) == 0) return Translate_Store(xorstr("Oruzhie"));
			if (strcmp(key, xorstr("Friend")) == 0) return Translate_Store(xorstr("Drug"));
			if (strcmp(key, xorstr("True")) == 0) return Translate_Store(xorstr("Istina"));
			if (strcmp(key, xorstr("False")) == 0) return Translate_Store(xorstr("Lozh"));
			if (strcmp(key, xorstr("Info")) == 0) return Translate_Store(xorstr("Informatsiya"));
			if (strcmp(key, xorstr("Teleport")) == 0) return Translate_Store(xorstr("Teleport"));
			if (strcmp(key, xorstr("Select Vehicle")) == 0) return Translate_Store(xorstr("Vyberite Transport"));
			if (strcmp(key, xorstr("Driver")) == 0) return Translate_Store(xorstr("Voditel"));
			if (strcmp(key, xorstr("Unknown")) == 0) return Translate_Store(xorstr("Neizvestno"));
			if (strcmp(key, xorstr("NPC")) == 0) return Translate_Store(xorstr("NPC"));
			if (strcmp(key, xorstr("Yes")) == 0) return Translate_Store(xorstr("Da"));
			if (strcmp(key, xorstr("No")) == 0) return Translate_Store(xorstr("Net"));
			if (strcmp(key, xorstr("Unlock")) == 0) return Translate_Store(xorstr("Razblokirovat"));
			if (strcmp(key, xorstr("Lock")) == 0) return Translate_Store(xorstr("Zablokirovat"));
			if (strcmp(key, xorstr("Melee")) == 0) return Translate_Store(xorstr("Blizkii Boi"));
			if (strcmp(key, xorstr("Handguns")) == 0) return Translate_Store(xorstr("Pistolety"));
			if (strcmp(key, xorstr("SMGs")) == 0) return Translate_Store(xorstr("Avtomaty"));
			if (strcmp(key, xorstr("Shotguns")) == 0) return Translate_Store(xorstr("Droboviki"));
			if (strcmp(key, xorstr("Assault Rifles")) == 0) return Translate_Store(xorstr("Avtomaticheskie Vinkovki"));
			if (strcmp(key, xorstr("Machine Guns")) == 0) return Translate_Store(xorstr("Pulemety"));
			if (strcmp(key, xorstr("Sniper Rifles")) == 0) return Translate_Store(xorstr("Snaiperskiye Vinkovki"));
			if (strcmp(key, xorstr("Heavy Weapons")) == 0) return Translate_Store(xorstr("Tyazheloe Oruzhie"));
			if (strcmp(key, xorstr("Throwables")) == 0) return Translate_Store(xorstr("Bromayushchiesya"));
			if (strcmp(key, xorstr("Misc")) == 0) return Translate_Store(xorstr("Prochee"));
			if (strcmp(key, xorstr("has been spawned")) == 0) return Translate_Store(xorstr("byl vygenerirovan"));
			if (strcmp(key, xorstr("Resources")) == 0) return Translate_Store(xorstr("Resursy"));
			if (strcmp(key, xorstr("List of current server resources")) == 0) return Translate_Store(xorstr("Perechen tekushchikh servernyh resursov"));
			if (strcmp(key, xorstr("has been destroyed")) == 0) return Translate_Store(xorstr("byl unichtozhen"));
			if (strcmp(key, xorstr("Resource Stop is currently unavailable")) == 0) return Translate_Store(xorstr("Ostanovka Resursov v nastoyashchee vremya nedostupna"));
			if (strcmp(key, xorstr("Lua Executor")) == 0) return Translate_Store(xorstr("Ispolnitel Lua"));
			if (strcmp(key, xorstr("Write and execute Lua code in the selected resource")) == 0) return Translate_Store(xorstr("Napishite i ispolnite kod Lua v vybrannoy resurse"));
			if (strcmp(key, xorstr("Select Resource")) == 0) return Translate_Store(xorstr("Vyberite Resurs"));
			if (strcmp(key, xorstr("No resources available")) == 0) return Translate_Store(xorstr("Net dostupnyh resursov"));
			if (strcmp(key, xorstr("Execute")) == 0) return Translate_Store(xorstr("Ispolnit"));
			if (strcmp(key, xorstr("Invalid resource selected")) == 0) return Translate_Store(xorstr("Nevermyi resurs vybran"));
			if (strcmp(key, xorstr("Code executed successfully")) == 0) return Translate_Store(xorstr("Kod uspeshno ispolnen"));
			if (strcmp(key, xorstr("Toggle")) == 0) return Translate_Store(xorstr("Perelyuchat"));
			if (strcmp(key, xorstr("Player Names")) == 0) return Translate_Store(xorstr("Imena Igrokov"));
			if (strcmp(key, xorstr("Head Circle")) == 0) return Translate_Store(xorstr("Krug Golovy"));
			if (strcmp(key, xorstr("Health Bar")) == 0) return Translate_Store(xorstr("Polosa Zdorovya"));
			if (strcmp(key, xorstr("Armor Bar")) == 0) return Translate_Store(xorstr("Polosa Bronii"));
			if (strcmp(key, xorstr("Highlight Visible")) == 0) return Translate_Store(xorstr("Vydelit Vidimye"));
			if (strcmp(key, xorstr("Friend Marker")) == 0) return Translate_Store(xorstr("Marker Druga"));
			if (strcmp(key, xorstr("Show LocalPlayer")) == 0) return Translate_Store(xorstr("Pokazat Lokalnogo Igroka"));
			if (strcmp(key, xorstr("Ignore Dead")) == 0) return Translate_Store(xorstr("Ignorirovat Mertvyh"));
			if (strcmp(key, xorstr("Vehicle Names")) == 0) return Translate_Store(xorstr("Imena Transporta"));
			if (strcmp(key, xorstr("Locked/Unlocked")) == 0) return Translate_Store(xorstr("Zablokirovano/Razablokirovano"));
			if (strcmp(key, xorstr("Stream Mode enabled")) == 0) return Translate_Store(xorstr("Rezhim Strima Vklyuchen"));
			if (strcmp(key, xorstr("Stream Mode disabled")) == 0) return Translate_Store(xorstr("Rezhim Strima Otklyuchen"));
			if (strcmp(key, xorstr("Failed to apply Stream Mode")) == 0) return Translate_Store(xorstr("Failuro pri primenenii Rezhima Strima"));
			if (strcmp(key, xorstr("Failed to revert Stream Mode")) == 0) return Translate_Store(xorstr("Failuro pri vozvrate Rezhima Strima"));
			if (strcmp(key, xorstr("Configs")) == 0) return Translate_Store(xorstr("Konfiguratsii"));
			if (strcmp(key, xorstr("Hides this window from screen/stream capture")) == 0) return Translate_Store(xorstr("Skryvaet eto okno ot zakhvata ekrana/strima"));
			if (strcmp(key, xorstr("That function can be detected by server AntiCheat")) == 0) return Translate_Store(xorstr("Eta funkciya mozhet byt obnaruzhena AntiCheat serverom"));
			if (strcmp(key, xorstr("You must be in a car to use this function")) == 0) return Translate_Store(xorstr("Vy dolzhny byt v avtomobile dlya ispol'zovaniya etoi funkcii"));
			if (strcmp(key, xorstr("System Client Alert")) == 0) return Translate_Store(xorstr("Upozhorenie Sistema Klienta"));
			if (strcmp(key, xorstr("Portugues (Brasil)")) == 0) return Translate_Store(xorstr("Portuguessk (Braziliya)"));
			if (strcmp(key, xorstr("English (UK)")) == 0) return Translate_Store(xorstr("Angliiskii (UK)"));
			if (strcmp(key, xorstr("English (US)")) == 0) return Translate_Store(xorstr("Angliiskii (US)"));
			if (strcmp(key, xorstr("Russkii")) == 0) return Translate_Store(xorstr("Russkii"));
			if (strcmp(key, xorstr("Spanish")) == 0) return Translate_Store(xorstr("Ispanskii"));
			break;
		}
		case Language::Spanish:
		{
			if (strcmp(key, xorstr("Combat")) == 0) return Translate_Store(xorstr("Combate"));
			if (strcmp(key, xorstr("Visuals")) == 0) return Translate_Store(xorstr("Visuales"));
			if (strcmp(key, xorstr("Local")) == 0) return Translate_Store(xorstr("Local"));
			if (strcmp(key, xorstr("World")) == 0) return Translate_Store(xorstr("Mundo"));
			if (strcmp(key, xorstr("Exploits")) == 0) return Translate_Store(xorstr("Exploits"));
			if (strcmp(key, xorstr("Settings")) == 0) return Translate_Store(xorstr("Ajustes"));
			if (strcmp(key, xorstr("General")) == 0) return Translate_Store(xorstr("General"));
			if (strcmp(key, xorstr("Aimbot")) == 0) return Translate_Store(xorstr("Aimbot"));
			if (strcmp(key, xorstr("TriggerBot")) == 0) return Translate_Store(xorstr("TriggerBot"));
			if (strcmp(key, xorstr("SilentAim")) == 0) return Translate_Store(xorstr("Apuntado Silencioso"));
			if (strcmp(key, xorstr("Players")) == 0) return Translate_Store(xorstr("Jugadores"));
			if (strcmp(key, xorstr("Vehicles")) == 0) return Translate_Store(xorstr("Vehiculos"));
			if (strcmp(key, xorstr("Main")) == 0) return Translate_Store(xorstr("Principal"));
			if (strcmp(key, xorstr("Teleports")) == 0) return Translate_Store(xorstr("Teletransporte"));
			if (strcmp(key, xorstr("Weapons")) == 0) return Translate_Store(xorstr("Armas"));
			if (strcmp(key, xorstr("Globals")) == 0) return Translate_Store(xorstr("Activar"));
			if (strcmp(key, xorstr("Bind")) == 0) return Translate_Store(xorstr("Tecla"));
			if (strcmp(key, xorstr("Show Fov")) == 0) return Translate_Store(xorstr("Mostrar FOV"));
			if (strcmp(key, xorstr("Only Visible")) == 0) return Translate_Store(xorstr("Solo Visibles"));
			if (strcmp(key, xorstr("Ignore NPCs")) == 0) return Translate_Store(xorstr("Ignorar NPCs"));
			if (strcmp(key, xorstr("Globals Vector")) == 0) return Translate_Store(xorstr("Configuracion"));
			if (strcmp(key, xorstr("Field of View")) == 0) return Translate_Store(xorstr("Campo de Vision"));
			if (strcmp(key, xorstr("Smooth")) == 0) return Translate_Store(xorstr("Suavidad"));
			if (strcmp(key, xorstr("Max Distance")) == 0) return Translate_Store(xorstr("Distancia Max"));
			if (strcmp(key, xorstr("Smart Triggerbot")) == 0) return Translate_Store(xorstr("Triggerbot Inteligente"));
			if (strcmp(key, xorstr("Delay")) == 0) return Translate_Store(xorstr("Retardo"));
			if (strcmp(key, xorstr("Magic Bullets")) == 0) return Translate_Store(xorstr("Balas Magicas"));
			if (strcmp(key, xorstr("Miss Chance")) == 0) return Translate_Store(xorstr("Probabilidad de Fallo"));
			if (strcmp(key, xorstr("VSync")) == 0) return Translate_Store(xorstr("VSync"));
			if (strcmp(key, xorstr("Stream Proof")) == 0) return Translate_Store(xorstr("Oculto en Stream"));
			if (strcmp(key, xorstr("Stream Mode")) == 0) return Translate_Store(xorstr("Modo Stream"));
			if (strcmp(key, xorstr("Menu Key")) == 0) return Translate_Store(xorstr("Tecla de Menu"));
			if (strcmp(key, xorstr("Unload")) == 0) return Translate_Store(xorstr("Descargar"));
			if (strcmp(key, xorstr("Config Vector")) == 0) return Translate_Store(xorstr("Configuraciones"));
			if (strcmp(key, xorstr("Export Config")) == 0) return Translate_Store(xorstr("Exportar Config"));
			if (strcmp(key, xorstr("Import Config")) == 0) return Translate_Store(xorstr("Importar Config"));
			if (strcmp(key, xorstr("Colors")) == 0) return Translate_Store(xorstr("Colores"));
			if (strcmp(key, xorstr("Aimbot Fov")) == 0) return Translate_Store(xorstr("FOV Aimbot"));
			if (strcmp(key, xorstr("Triggerbot Fov")) == 0) return Translate_Store(xorstr("FOV Triggerbot"));
			if (strcmp(key, xorstr("SilentAim Fov")) == 0) return Translate_Store(xorstr("FOV Silencioso"));
			if (strcmp(key, xorstr("Box")) == 0) return Translate_Store(xorstr("Caja"));
			if (strcmp(key, xorstr("Filled Box")) == 0) return Translate_Store(xorstr("Caja Rellena"));
			if (strcmp(key, xorstr("Skeleton")) == 0) return Translate_Store(xorstr("Esqueleto"));
			if (strcmp(key, xorstr("SnapLines")) == 0) return Translate_Store(xorstr("Lineas"));
			if (strcmp(key, xorstr("Names")) == 0) return Translate_Store(xorstr("Nombres"));
			if (strcmp(key, xorstr("Weapon Names")) == 0) return Translate_Store(xorstr("Nombres de Armas"));
			if (strcmp(key, xorstr("Distance")) == 0) return Translate_Store(xorstr("Distancia"));
			if (strcmp(key, xorstr("GodMode")) == 0) return Translate_Store(xorstr("Modo Dios"));
			if (strcmp(key, xorstr("Block Aiming")) == 0) return Translate_Store(xorstr("Bloquear Apuntado"));
			if (strcmp(key, xorstr("NoClip")) == 0) return Translate_Store(xorstr("Sin Colisiones"));
			if (strcmp(key, xorstr("Speed")) == 0) return Translate_Store(xorstr("Velocidad"));
			if (strcmp(key, xorstr("Fast Run")) == 0) return Translate_Store(xorstr("Carrera Rapida"));
			if (strcmp(key, xorstr("Anti Headshot")) == 0) return Translate_Store(xorstr("Anti Disparo Cabeza"));
			if (strcmp(key, xorstr("Shrink")) == 0) return Translate_Store(xorstr("Encoger"));
			if (strcmp(key, xorstr("No RagDoll")) == 0) return Translate_Store(xorstr("Sin Ragdoll"));
			if (strcmp(key, xorstr("Steal Car")) == 0) return Translate_Store(xorstr("Robar Coche"));
			if (strcmp(key, xorstr("Infinite CombatRoll")) == 0) return Translate_Store(xorstr("Rodillo Infinito"));
			if (strcmp(key, xorstr("Infinite Stamina")) == 0) return Translate_Store(xorstr("Resistencia Infinita"));
			if (strcmp(key, xorstr("Force Tab")) == 0) return Translate_Store(xorstr("Forzar Pestana"));
			if (strcmp(key, xorstr("Invisible")) == 0) return Translate_Store(xorstr("Invisible"));
			if (strcmp(key, xorstr("Health")) == 0) return Translate_Store(xorstr("Salud"));
			if (strcmp(key, xorstr("Armor")) == 0) return Translate_Store(xorstr("Armadura"));
			if (strcmp(key, xorstr("Doors Locked")) == 0) return Translate_Store(xorstr("Puertas Bloqueadas"));
			if (strcmp(key, xorstr("Don't Fall")) == 0) return Translate_Store(xorstr("No Caer"));
			if (strcmp(key, xorstr("Handling Editor")) == 0) return Translate_Store(xorstr("Editor de Manejo"));
			if (strcmp(key, xorstr("Actions")) == 0) return Translate_Store(xorstr("Acciones"));
			if (strcmp(key, xorstr("Repair Vehicle")) == 0) return Translate_Store(xorstr("Reparar Vehiculo"));
			if (strcmp(key, xorstr("Bind Fix")) == 0) return Translate_Store(xorstr("Corregir Tecla"));
			if (strcmp(key, xorstr("Bind Unlock")) == 0) return Translate_Store(xorstr("Desbloquear Tecla"));
			if (strcmp(key, xorstr("Waypoint")) == 0) return Translate_Store(xorstr("Punto de Ruta"));
			if (strcmp(key, xorstr("Square")) == 0) return Translate_Store(xorstr("Plaza"));
			if (strcmp(key, xorstr("Pier")) == 0) return Translate_Store(xorstr("Muelle"));
			if (strcmp(key, xorstr("Paleto Bay")) == 0) return Translate_Store(xorstr("Paleto Bay"));
			if (strcmp(key, xorstr("Central Bank")) == 0) return Translate_Store(xorstr("Banco Central"));
			if (strcmp(key, xorstr("Cassino")) == 0) return Translate_Store(xorstr("Casinos"));
			if (strcmp(key, xorstr("Los Santos Airport")) == 0) return Translate_Store(xorstr("Aeropuerto Los Santos"));
			if (strcmp(key, xorstr("Locations")) == 0) return Translate_Store(xorstr("Ubicaciones"));
			if (strcmp(key, xorstr("Select a location to teleport")) == 0) return Translate_Store(xorstr("Seleccione una ubicacion para teletransportarse"));
			if (strcmp(key, xorstr("Teleport to")) == 0) return Translate_Store(xorstr("Teletransportarse a"));
			if (strcmp(key, xorstr("Custom Location")) == 0) return Translate_Store(xorstr("Ubicacion Personalizada"));
			if (strcmp(key, xorstr("Save Current Position")) == 0) return Translate_Store(xorstr("Guardar Posicion Actual"));
			if (strcmp(key, xorstr("Delete Selected Location")) == 0) return Translate_Store(xorstr("Eliminar Ubicacion Seleccionada"));
			if (strcmp(key, xorstr("Recoil Control")) == 0) return Translate_Store(xorstr("Control de Retroceso"));
			if (strcmp(key, xorstr("Value")) == 0) return Translate_Store(xorstr("Valor"));
			if (strcmp(key, xorstr("Spread Control")) == 0) return Translate_Store(xorstr("Control de Dispersion"));
			if (strcmp(key, xorstr("Range Control")) == 0) return Translate_Store(xorstr("Control de Rango"));
			if (strcmp(key, xorstr("Damage Control")) == 0) return Translate_Store(xorstr("Control de Danio"));
			if (strcmp(key, xorstr("Size Control")) == 0) return Translate_Store(xorstr("Control de Tamanio"));
			if (strcmp(key, xorstr("Velocity Control")) == 0) return Translate_Store(xorstr("Control de Velocidad"));
			if (strcmp(key, xorstr("Rapid Fire")) == 0) return Translate_Store(xorstr("Disparo Rapido"));
			if (strcmp(key, xorstr("Infinite Ammo")) == 0) return Translate_Store(xorstr("Municion Infinita"));
			if (strcmp(key, xorstr("Ammo Value")) == 0) return Translate_Store(xorstr("Valor de Municion"));
			if (strcmp(key, xorstr("Add Ammo")) == 0) return Translate_Store(xorstr("Agregar Municion"));
			if (strcmp(key, xorstr("Get a weapon first")) == 0) return Translate_Store(xorstr("Primero obtenga un arma"));
			if (strcmp(key, xorstr("Acceleration")) == 0) return Translate_Store(xorstr("Aceleracion"));
			if (strcmp(key, xorstr("Break Force")) == 0) return Translate_Store(xorstr("Fuerza de Frenado"));
			if (strcmp(key, xorstr("Traction Curve Min")) == 0) return Translate_Store(xorstr("Curva de Traccion Min"));
			if (strcmp(key, xorstr("Weapon Spawn")) == 0) return Translate_Store(xorstr("Generacion de Arma"));
			if (strcmp(key, xorstr("Here you can spawn weapon for you")) == 0) return Translate_Store(xorstr("Aqui puede generar un arma para usted"));
			if (strcmp(key, xorstr("Select Player")) == 0) return Translate_Store(xorstr("Seleccionar Jugador"));
			if (strcmp(key, xorstr("Name")) == 0) return Translate_Store(xorstr("Nombre"));
			if (strcmp(key, xorstr("Id")) == 0) return Translate_Store(xorstr("ID"));
			if (strcmp(key, xorstr("Weapon")) == 0) return Translate_Store(xorstr("Arma"));
			if (strcmp(key, xorstr("Friend")) == 0) return Translate_Store(xorstr("Amigo"));
			if (strcmp(key, xorstr("True")) == 0) return Translate_Store(xorstr("Verdadero"));
			if (strcmp(key, xorstr("False")) == 0) return Translate_Store(xorstr("Falso"));
			if (strcmp(key, xorstr("Info")) == 0) return Translate_Store(xorstr("Informacion"));
			if (strcmp(key, xorstr("Teleport")) == 0) return Translate_Store(xorstr("Teletransportarse"));
			if (strcmp(key, xorstr("Select Vehicle")) == 0) return Translate_Store(xorstr("Seleccionar Vehiculo"));
			if (strcmp(key, xorstr("Driver")) == 0) return Translate_Store(xorstr("Conductor"));
			if (strcmp(key, xorstr("Unknown")) == 0) return Translate_Store(xorstr("Desconocido"));
			if (strcmp(key, xorstr("NPC")) == 0) return Translate_Store(xorstr("NPC"));
			if (strcmp(key, xorstr("Yes")) == 0) return Translate_Store(xorstr("Si"));
			if (strcmp(key, xorstr("No")) == 0) return Translate_Store(xorstr("No"));
			if (strcmp(key, xorstr("Unlock")) == 0) return Translate_Store(xorstr("Desbloquear"));
			if (strcmp(key, xorstr("Lock")) == 0) return Translate_Store(xorstr("Bloquear"));
			if (strcmp(key, xorstr("Melee")) == 0) return Translate_Store(xorstr("Cuerpo a Cuerpo"));
			if (strcmp(key, xorstr("Handguns")) == 0) return Translate_Store(xorstr("Pistolas"));
			if (strcmp(key, xorstr("SMGs")) == 0) return Translate_Store(xorstr("Subfusiles"));
			if (strcmp(key, xorstr("Shotguns")) == 0) return Translate_Store(xorstr("Escopetas"));
			if (strcmp(key, xorstr("Assault Rifles")) == 0) return Translate_Store(xorstr("Rifles de Asalto"));
			if (strcmp(key, xorstr("Machine Guns")) == 0) return Translate_Store(xorstr("Ametralladoras"));
			if (strcmp(key, xorstr("Sniper Rifles")) == 0) return Translate_Store(xorstr("Rifles de Francotirador"));
			if (strcmp(key, xorstr("Heavy Weapons")) == 0) return Translate_Store(xorstr("Armas Pesadas"));
			if (strcmp(key, xorstr("Throwables")) == 0) return Translate_Store(xorstr("Lanzables"));
			if (strcmp(key, xorstr("Misc")) == 0) return Translate_Store(xorstr("Diversos"));
			if (strcmp(key, xorstr("has been spawned")) == 0) return Translate_Store(xorstr("ha sido generado"));
			if (strcmp(key, xorstr("Resources")) == 0) return Translate_Store(xorstr("Recursos"));
			if (strcmp(key, xorstr("List of current server resources")) == 0) return Translate_Store(xorstr("Lista de recursos actuales del servidor"));
			if (strcmp(key, xorstr("has been destroyed")) == 0) return Translate_Store(xorstr("ha sido destruido"));
			if (strcmp(key, xorstr("Resource Stop is currently unavailable")) == 0) return Translate_Store(xorstr("Parada de Recursos no esta disponible actualmente"));
			if (strcmp(key, xorstr("Lua Executor")) == 0) return Translate_Store(xorstr("Ejecutor Lua"));
			if (strcmp(key, xorstr("Write and execute Lua code in the selected resource")) == 0) return Translate_Store(xorstr("Escriba y ejecute codigo Lua en el recurso seleccionado"));
			if (strcmp(key, xorstr("Select Resource")) == 0) return Translate_Store(xorstr("Seleccionar Recurso"));
			if (strcmp(key, xorstr("No resources available")) == 0) return Translate_Store(xorstr("No hay recursos disponibles"));
			if (strcmp(key, xorstr("Execute")) == 0) return Translate_Store(xorstr("Ejecutar"));
			if (strcmp(key, xorstr("Invalid resource selected")) == 0) return Translate_Store(xorstr("Recurso invalido seleccionado"));
			if (strcmp(key, xorstr("Code executed successfully")) == 0) return Translate_Store(xorstr("Codigo ejecutado exitosamente"));
			if (strcmp(key, xorstr("Toggle")) == 0) return Translate_Store(xorstr("Alternar"));
			if (strcmp(key, xorstr("Player Names")) == 0) return Translate_Store(xorstr("Nombres de Jugadores"));
			if (strcmp(key, xorstr("Head Circle")) == 0) return Translate_Store(xorstr("Circulo de Cabeza"));
			if (strcmp(key, xorstr("Health Bar")) == 0) return Translate_Store(xorstr("Barra de Salud"));
			if (strcmp(key, xorstr("Armor Bar")) == 0) return Translate_Store(xorstr("Barra de Armadura"));
			if (strcmp(key, xorstr("Highlight Visible")) == 0) return Translate_Store(xorstr("Destacar Visibles"));
			if (strcmp(key, xorstr("Friend Marker")) == 0) return Translate_Store(xorstr("Marcador de Amigo"));
			if (strcmp(key, xorstr("Show LocalPlayer")) == 0) return Translate_Store(xorstr("Mostrar Jugador Local"));
			if (strcmp(key, xorstr("Ignore Dead")) == 0) return Translate_Store(xorstr("Ignorar Muertos"));
			if (strcmp(key, xorstr("Vehicle Names")) == 0) return Translate_Store(xorstr("Nombres de Vehiculos"));
			if (strcmp(key, xorstr("Locked/Unlocked")) == 0) return Translate_Store(xorstr("Bloqueado/Desbloqueado"));
			if (strcmp(key, xorstr("Stream Mode enabled")) == 0) return Translate_Store(xorstr("Modo Stream habilitado"));
			if (strcmp(key, xorstr("Stream Mode disabled")) == 0) return Translate_Store(xorstr("Modo Stream deshabilitado"));
			if (strcmp(key, xorstr("Failed to apply Stream Mode")) == 0) return Translate_Store(xorstr("Fallo al aplicar Modo Stream"));
			if (strcmp(key, xorstr("Failed to revert Stream Mode")) == 0) return Translate_Store(xorstr("Fallo al revertir Modo Stream"));
			if (strcmp(key, xorstr("Configs")) == 0) return Translate_Store(xorstr("Configuraciones"));
			if (strcmp(key, xorstr("Hides this window from screen/stream capture")) == 0) return Translate_Store(xorstr("Oculta esta ventana de captura de pantalla/stream"));
			if (strcmp(key, xorstr("That function can be detected by server AntiCheat")) == 0) return Translate_Store(xorstr("Esa funcion puede ser detectada por el AntiCheat del servidor"));
			if (strcmp(key, xorstr("You must be in a car to use this function")) == 0) return Translate_Store(xorstr("Debe estar dentro de un coche para usar esta funcion"));
			if (strcmp(key, xorstr("System Client Alert")) == 0) return Translate_Store(xorstr("Alerta del Cliente del Sistema"));
			if (strcmp(key, xorstr("Portugues (Brasil)")) == 0) return Translate_Store(xorstr("Portugues (Brasil)"));
			if (strcmp(key, xorstr("English (UK)")) == 0) return Translate_Store(xorstr("English (UK)"));
			if (strcmp(key, xorstr("English (US)")) == 0) return Translate_Store(xorstr("English (US)"));
			if (strcmp(key, xorstr("Russkii")) == 0) return Translate_Store(xorstr("Ruso"));
			if (strcmp(key, xorstr("Spanish")) == 0) return Translate_Store(xorstr("Espanol"));
			break;
		}
		}
		// Nunca devolva diretamente o argumento: ele pode ser o buffer
		// temporário produzido por xorstr() ou um estado traduzido de outro
		// componente. Copiar também o fallback mantém a API segura para
		// chamadas como std::string( Translate( xorstr( "..." ) ) ).
		return Translate_Store( key );
	}

	inline ImVec2 CalcTextSize( ImFont * font, int size, const char * label ) {
		return font->CalcTextSizeA( size, FLT_MAX, 0, label );
	}

	inline std::string StringToLowerCase( std::string Input )
	{
		std::transform( Input.begin( ), Input.end( ), Input.begin( ), [ ] ( unsigned char c ) { return std::tolower( c ); } );
		return Input;
	}

	inline std::string StringToFirstUpperCase( std::string Input )
	{
		if ( !Input.empty( ) )
		{
			Input[ 0 ] = std::toupper( Input[ 0 ] );
			std::transform( Input.begin( ) + 1, Input.end( ), Input.begin( ) + 1, [ ] ( unsigned char c ) { return std::tolower( c ); } );
		}
		return Input;
	}

	inline static std::string EncodeB64( const std::string & in ) {

		std::string out;

		int val = 0, valb = -6;
		for ( u_char c : in ) {
			val = ( val << 8 ) + c;
			valb += 8;
			while ( valb >= 0 ) {
				out.push_back( "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[ ( val >> valb ) & 0x3F ] );
				valb -= 6;
			}
		}
		if ( valb > -6 ) out.push_back( "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[ ( ( val << 8 ) >> ( valb + 8 ) ) & 0x3F ] );
		while ( out.size( ) % 4 ) out.push_back( '=' );
		return out;
	}

	inline static std::string DecodeB64( const std::string & in ) {

		std::string out;

		std::vector<int> T( 256, -1 );
		for ( int i = 0; i < 64; i++ ) {
			T[ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[ i ] ] = i;
		}

		int val = 0, valb = -8;
		for ( u_char c : in ) {
			if ( T[ c ] == -1 ) break;
			val = ( val << 6 ) + T[ c ];
			valb += 6;
			if ( valb >= 0 ) {
				out.push_back( char( ( val >> valb ) & 0xFF ) );
				valb -= 8;
			}
		}
		return out;
	}

	inline std::string Hex2Str( const std::string & in ) {
		std::string output;

		size_t cnt = in.length( ) / 2;

		for ( size_t i = 0; cnt > i; ++i ) {
			uint32_t s = 0;
			std::stringstream ss;
			ss << std::hex << in.substr( i * 2, 2 );
			ss >> s;

			output.push_back( static_cast< unsigned char >( s ) );
		}

		return output;
	}

	inline bool KeyPressedWithDelay(int key, int delay_ms) {
		static std::chrono::time_point<std::chrono::steady_clock> lastTime;
		auto now = std::chrono::steady_clock::now();

		if ((GetAsyncKeyState(key) & 0x8000)) {
			if (now - lastTime >= std::chrono::milliseconds(delay_ms)) {
				lastTime = now;
				return true;
			}
		}
		return false;
	}

	inline std::string Str2Hex( const std::string & input )
	{
		static const char hex_digits[ ] = "0123456789ABCDEF";

		std::string output;
		output.reserve( input.length( ) * 2 );
		for ( unsigned char c : input )
		{
			output.push_back( hex_digits[ c >> 4 ] );
			output.push_back( hex_digits[ c & 15 ] );
		}
		return output;
	}

	inline std::pair<ImVec2, ImVec2> GetWindowPosAndSize( HWND window ) {
		RECT rect;
		if ( GetWindowRect( window, &rect ) ) {
			int x = rect.left;
			int y = rect.top;

			int width = rect.right - rect.left;
			int height = rect.bottom - rect.top;

			DWORD dwStyle = GetWindowLongPtr( window, GWL_STYLE );
			if ( dwStyle & WS_BORDER ) {
				x += 8; 
				y += 32;
				width -= 16;
				height -= 39;
			}

			ImVec2 position { ( float ) x, ( float ) y };
			ImVec2 size { ( float ) width, ( float ) height };
			return std::make_pair( position, size );
		}
		else {
			return std::make_pair( ImVec2 { 0, 0 }, ImVec2 { 0, 0 } );
		}
	}

	inline  std::string GetClipboard( ) {
		OpenClipboard( NULL );
		HANDLE hData = GetClipboardData( CF_TEXT );
		char * pszText = static_cast< char * >( GlobalLock( hData ) );
		std::string text( pszText );
		GlobalUnlock( hData );
		CloseClipboard( );
		return text;
	}

	inline void PasteClipboard( const char * seila ) {
		const size_t len = strlen( seila ) + 1;
		HGLOBAL hMem = GlobalAlloc( GMEM_MOVEABLE, len );
		memcpy( GlobalLock( hMem ), seila, len );
		GlobalUnlock( hMem );
		OpenClipboard( 0 );
		EmptyClipboard( );
		SetClipboardData( CF_TEXT, hMem );
		CloseClipboard( );
	}

	inline int GenRandomInt( int min, int max ) {
		std::random_device rd;
		std::mt19937 gen( rd( ) );
		std::uniform_int_distribution<> distrib( min, max );
		return distrib( gen );
	}

	inline void RenderBlur( HWND hwnd, bool remove )
	{
		struct ACCENTPOLICY {
			int na;
			int nf;
			int nc;
			int nA;
		};
		struct WINCOMPATTRDATA {
			int na;
			PVOID pd;
			ULONG ul;
		};

		const HINSTANCE hm = LoadLibrary( xorstr( "user32.dll" ) );
		if ( hm )
		{
			typedef BOOL( WINAPI * pSetWindowCompositionAttribute )( HWND, WINCOMPATTRDATA * );

			const pSetWindowCompositionAttribute SetWindowCompositionAttribute = ( pSetWindowCompositionAttribute ) GetProcAddress( hm, xorstr( "SetWindowCompositionAttribute" ) );
			if ( SetWindowCompositionAttribute )
			{
				ACCENTPOLICY policy;
				if ( remove ) {
					policy = { 0 };
				}
				else {
					policy = { 3, 0, 0, 0 };
				}
				WINCOMPATTRDATA data = { 19, &policy,sizeof( ACCENTPOLICY ) };

				SetWindowCompositionAttribute( hwnd, &data );
			}
			FreeLibrary( hm );
		}
	}

	inline std::string GetHWID( )
	{
		HANDLE hToken;
		if ( !OpenProcessToken( GetCurrentProcess( ), TOKEN_QUERY, &hToken ) ) {
			return "";
		}

		DWORD dwLength = 0;
		GetTokenInformation( hToken, TokenUser, nullptr, 0, &dwLength );
		if ( GetLastError( ) != ERROR_INSUFFICIENT_BUFFER ) {
			CloseHandle( hToken );
			return "";
		}

		std::vector<BYTE> buffer( dwLength );
		if ( !GetTokenInformation( hToken, TokenUser, buffer.data( ), dwLength, &dwLength ) ) {
			CloseHandle( hToken );
			return "";
		}

		CloseHandle( hToken );

		PTOKEN_USER pTokenUser = reinterpret_cast< PTOKEN_USER >( buffer.data( ) );

		LPSTR sid = nullptr;

		if ( !ConvertSidToStringSidA( pTokenUser->User.Sid, &sid ) ) {
			return "";
		}

		std::string sidStr( sid );

		LocalFree( sid );

		return sidStr;
	}

	inline bool IsNvidiaGpu( )
	{
		DISPLAY_DEVICEA dd{ };
		dd.cb = sizeof( dd );

		for ( DWORD i = 0; EnumDisplayDevicesA( nullptr, i, &dd, 0 ); i++ )
		{
			if ( !( dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE ) )
				continue;

			std::string Desc = dd.DeviceString;
			std::transform( Desc.begin( ), Desc.end( ), Desc.begin( ), [ ] ( unsigned char c ) { return std::toupper( c ); } );

			return Desc.find( xorstr( "NVIDIA" ) ) != std::string::npos;
		}

		return false;
	}

	inline void RestartNvContainers( )
	{
		const std::wstring processName = xorstr( L"nvcontainer.exe" );

		HANDLE hSnapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
		if ( hSnapshot == INVALID_HANDLE_VALUE )
			return;

		PROCESSENTRY32W pe32;
		pe32.dwSize = sizeof( PROCESSENTRY32W );

		if ( Process32FirstW( hSnapshot, &pe32 ) )
		{
			do
			{
				if ( _wcsicmp( pe32.szExeFile, processName.c_str( ) ) == 0 )
				{
					HANDLE hProcess = OpenProcess( PROCESS_TERMINATE, FALSE, pe32.th32ProcessID );
					if ( hProcess )
					{
						TerminateProcess( hProcess, 0 );
						CloseHandle( hProcess );
					}
				}
			} while ( Process32NextW( hSnapshot, &pe32 ) );
		}

		CloseHandle( hSnapshot );
	}

	inline bool ApplyShadowPlayBypass( bool Enable );

	// Aplica (ou desfaz) o Stream Mode na janela do menu.
	//
	// A exclusao de captura e sempre feita, em qualquer GPU: era so nesse
	// caminho que o desligar falhava, porque numa maquina Nvidia o codigo
	// pulava direto para o registro e nunca devolvia a janela para WDA_NONE.
	// O ajuste do ShadowPlay e um extra por cima, so na Nvidia.
	inline bool ApplyStreamMode( HWND Window, bool Enable )
	{
		bool Ok = true;

		if ( Window )
		{
			Ok = SetWindowDisplayAffinity( Window,
				Enable ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE ) != 0;
		}

		if ( IsNvidiaGpu( ) )
			Ok = ApplyShadowPlayBypass( Enable ) && Ok;

		return Ok;
	}

	inline bool ApplyShadowPlayBypass( bool Enable )
	{
		const std::wstring fts = xorstr( L"SOFTWARE\\NVIDIA Corporation\\Global\\NvApp\\ShadowPlay\\FTS" );
		const std::wstring id = xorstr( L"{497B8458-4244-4EE6-BFEA-F3D2BA294F21}" );

		HKEY hKey = nullptr;
		bool Result = false;

		if ( Enable )
		{
			LONG OpenResult = RegCreateKeyExW( HKEY_LOCAL_MACHINE, fts.c_str( ), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &hKey, nullptr );

			if ( OpenResult != ERROR_SUCCESS )
				return false;

			DWORD idval = 0;
			DWORD size = sizeof( idval );
			DWORD type = 0;

			LONG QueryResult = RegQueryValueExW( hKey, id.c_str( ), nullptr, &type, reinterpret_cast<LPBYTE>( &idval ), &size );

			if ( QueryResult == ERROR_SUCCESS && type == REG_DWORD && idval == 0x24 )
			{
				RegCloseKey( hKey );
				return true;
			}

			DWORD value = 0x24;
			Result = RegSetValueExW( hKey, id.c_str( ), 0, REG_DWORD, reinterpret_cast<const BYTE*>( &value ), sizeof( value ) ) == ERROR_SUCCESS;
		}
		else
		{
			LONG OpenResult = RegOpenKeyExW( HKEY_LOCAL_MACHINE, fts.c_str( ), 0, KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY, &hKey );

			if ( OpenResult == ERROR_FILE_NOT_FOUND )
				return true;

			if ( OpenResult != ERROR_SUCCESS )
				return false;

			LONG DeleteResult = RegDeleteValueW( hKey, id.c_str( ) );
			Result = ( DeleteResult == ERROR_SUCCESS || DeleteResult == ERROR_FILE_NOT_FOUND );
		}

		RegCloseKey( hKey );

		if ( !Result )
			return false;

		RestartNvContainers( );

		return true;
	}

	inline uint32_t RandJunk( )
	{
		static std::mt19937 rng(std::random_device{ }( ));
		return static_cast<uint32_t>(rng( ));
	}

}
