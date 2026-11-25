#pragma once
#include <cmath>
#include <array>
#include <chrono>
#include <functional>
#include <Includes/Includes.hpp>
#include <Core/SDK/Memory.hpp>
#include <Core/Offsets.hpp>
#include <Core/SDK/Structs/GameClasses.hpp>

#include <D3dx9math.h>

// Host Brave (Shift+F6): o modulo atual nao e o FiveM, entao nenhuma leitura
// de memoria do jogo pode ser feita a partir deste processo.
inline bool RunningInsideBraveHost()
{
    char moduleName[ MAX_PATH ] = { 0 };
    GetModuleBaseNameA( GetCurrentProcess(), nullptr, moduleName, MAX_PATH );
    return _stricmp( moduleName, "brave.exe" ) == 0;
}

namespace Core {

	namespace SDK {

		namespace Pointers {
			// Object Pointers
			inline CPedFactory* pWorld = nullptr;
			inline CPed * pLocalPlayer = nullptr;
			inline CReplayInterFace * pReplayInterFace = nullptr;
			inline uintptr_t pBlipList = 0, pViewPort = 0, pCamGamePlayDirector = 0;
		}

		namespace Game
		{
			struct cSkeleton_t {

				std::unordered_map<unsigned int, int> StoredBonesIdx;

				struct crSkeletonData_t {
					uintptr_t Ptr, m_BoneIdTable;
					unsigned int m_Used; // crSkeletonData->m_BoneIdTable.m_Used
					unsigned int m_NumBones; // crSkeletonData->m_NumBones
					unsigned short m_BoneIdTable_Slots;
				} crSkeletonData;

				uintptr_t m_pSkeleton, Arg2;
				D3DXMATRIX Arg1;
			};

			struct NetworkInfo {
				std::string UserName;
				std::string DiscordId;
				std::string SteamId;
			};

			struct Visibility
			{
				bool IsVisible;
				uint32_t QueryId;
				std::chrono::steady_clock::time_point LastCheck;
				static constexpr auto CacheTime = std::chrono::milliseconds(10);
			};

			struct EntityStruct {
				CPed * Ped;
				int Id;
				int Index;
				int PedType;			
				bool IsFriend;
				bool IsVisible;
				D3DXVECTOR3 Pos;
				float Health;
				float MaxHealth;
				float Armor;
				float Distance;
				std::string WeaponName;
				// Inicializado: no primeiro passe o NewEntity nao preenche
				// esse campo, e sem isso ele viria com lixo da stack.
				uint32_t WeaponHash = 0;
				NetworkInfo NetworkInfo;
				float HealthAnim;
			};

			struct EspAnim {
				bool CanFadeOut;
				float Health;
				float Armor;
				float Alpha;
			};

			struct VehicleStructure {
				CVehicle * Pointer;
				CPed * Driver;
				std::string Name;
				float Health;
				float Dist;
				bool IsLocked;
				D3DXVECTOR3 Pos;
			};

			inline std::vector<EntityStruct> EntityList;

			// O mutex mora junto do vetor de proposito. Antes ele era membro
			// de cVehicleList/cEntityList, entao GetClosestPed (aqui embaixo)
			// e o Esp nao conseguiam alcanca-lo sem include circular -- e
			// iteravam o vetor sem lock enquanto a thread de fundo fazia
			// "EntityList = std::move(Updated)", que libera o buffer antigo.
			// Era use-after-free: o lock existia, mas so um lado o respeitava.
			inline std::mutex EntityListMutex;

			inline std::vector<VehicleStructure> VehicleList;
			inline std::mutex VehicleListMutex;
			extern std::unordered_map<int, bool> FriendMap;

			inline bool LoadSkeleton(CPed* Ped, cSkeleton_t& Skeleton)
			{
				uintptr_t FragInst = Mem.Read<uintptr_t>((uintptr_t)Ped + g_Offsets.m_FragInst);
				if (!FragInst) return false;

				uintptr_t v9 = Mem.Read<uintptr_t>(FragInst + 0x68);
				if (!v9) return false;

				Skeleton.m_pSkeleton = Mem.Read<uintptr_t>(v9 + 0x178);
				if (!Skeleton.m_pSkeleton) return false;

				Skeleton.crSkeletonData.Ptr = Mem.Read<uintptr_t>(Skeleton.m_pSkeleton);
				if (!Skeleton.crSkeletonData.Ptr) return false;

				Skeleton.crSkeletonData.m_Used = Mem.Read<unsigned int>(Skeleton.crSkeletonData.Ptr + 0x1A);
				// short, e nao int: 0x5E tem 2 bytes de contagem e 0x60 ja e outro
				// campo. Lendo 4 bytes a contagem virava dezenas de milhares.
				Skeleton.crSkeletonData.m_NumBones = Mem.Read<unsigned short>(Skeleton.crSkeletonData.Ptr + 0x5E);
				Skeleton.crSkeletonData.m_BoneIdTable_Slots = Mem.Read<unsigned short>(Skeleton.crSkeletonData.Ptr + 0x18);

				if (!Skeleton.crSkeletonData.m_BoneIdTable_Slots) return false;

				Skeleton.crSkeletonData.m_BoneIdTable = Mem.Read<uintptr_t>(Skeleton.crSkeletonData.Ptr + 0x10);
				Skeleton.Arg1 = Mem.Read<D3DXMATRIX>(Mem.Read<uintptr_t>(Skeleton.m_pSkeleton + 0x8));
				Skeleton.Arg2 = Mem.Read<uintptr_t>(Skeleton.m_pSkeleton + 0x18);
				return true;
			}

			inline D3DXVECTOR2 ProjectWorldToScreen( D3DXVECTOR3 World,
				const D3DXMATRIX & ViewMatrix )
			{
				auto VecX = D3DXVECTOR4( ViewMatrix._21, ViewMatrix._22, ViewMatrix._23, ViewMatrix._24 ),
					VecY = D3DXVECTOR4( ViewMatrix._31, ViewMatrix._32, ViewMatrix._33, ViewMatrix._34 ),
					VecZ = D3DXVECTOR4( ViewMatrix._41, ViewMatrix._42, ViewMatrix._43, ViewMatrix._44 );

				D3DXVECTOR3 ScreenPos = D3DXVECTOR3(
					( VecX.x * World.x ) + ( VecX.y * World.y ) + ( VecX.z * World.z ) + VecX.w,
					( VecY.x * World.x ) + ( VecY.y * World.y ) + ( VecY.z * World.z ) + VecY.w,
					( VecZ.x * World.x ) + ( VecZ.y * World.y ) + ( VecZ.z * World.z ) + VecZ.w
				);

				if ( ScreenPos.z <= 0.1f )
					return D3DXVECTOR2( 0, 0 );

				ScreenPos.z = 1.0f / ScreenPos.z;
				ScreenPos.x *= ScreenPos.z;
				ScreenPos.y *= ScreenPos.z;

				ScreenPos.x += ( g_Variables.g_vGameWindowSize.x / 2 ) + float( 0.5f * ScreenPos.x * g_Variables.g_vGameWindowSize.x + 0.5f );
				ScreenPos.y = ( g_Variables.g_vGameWindowSize.y / 2 ) - float( 0.5f * ScreenPos.y * g_Variables.g_vGameWindowSize.y + 0.5f );
				return D3DXVECTOR2( ScreenPos.x, ScreenPos.y );
			}

			inline D3DXVECTOR2 WorldToScreen( D3DXVECTOR3 World )
			{
				D3DXMATRIX ViewMatrix = Mem.Read<D3DXMATRIX>( Core::SDK::Pointers::pViewPort + 0x24C );
				D3DXMatrixTranspose( &ViewMatrix, &ViewMatrix );
				return ProjectWorldToScreen( World, ViewMatrix );
			}

			// Matrizes de osso do ultimo ped processado por
			// CalculateDynamicPedBounds, para o desenho do esqueleto reaproveitar.
			inline std::vector<D3DXMATRIX> g_BoneMtx;
			inline cSkeleton_t g_BoneSkel;
			inline CPed * g_BoneOwner = nullptr;

			// Caixa de um ped em espaco de tela, calculada a partir do esqueleto.
			//
			// Left/Right/Top/Bottom sao os extremos CRUS dos ossos, sem folga:
			// quem desenha decide a folga, porque ela depende do que vai ser
			// desenhado em volta.
			struct PedBounds
			{
				float Left, Right, Top, Bottom;
				float Width, Height;
			};

			inline bool GetBoneIndex(const cSkeleton_t& Skeleton, unsigned int BoneId, int& Index);

			// Use body bones for humanoids; helper/IK bones can extend outside
			// the visible body. Other skeletons retain the full-bone fallback.
			inline bool CalculateDynamicPedBounds( CPed * Ped, PedBounds & Out )
			{
				cSkeleton_t Skel;

				if ( !LoadSkeleton( Ped, Skel ) || !Skel.Arg2 )
					return false;

				const int NumBones = ( int ) Skel.crSkeletonData.m_NumBones;

				if ( NumBones <= 0 || NumBones > 512 )
					return false;

				// Chamado so pela thread de render, entao o buffer e
				// reaproveitado entre os peds em vez de realocar por ped.
				g_BoneMtx.resize( NumBones );
				std::vector<D3DXMATRIX> & BoneMtx = g_BoneMtx;

				if ( !Mem.ReadBuffer( Skel.Arg2, BoneMtx.data( ), NumBones * sizeof( D3DXMATRIX ) ) )
					return false;

				const D3DXMATRIX & M = Skel.Arg1;

				float MinX = FLT_MAX, MaxX = -FLT_MAX;
				float MinY = FLT_MAX, MaxY = -FLT_MAX;
				int Valid = 0;

				const unsigned int BodyBones[] = {
					SKEL_Head, SKEL_Neck_1, SKEL_Spine0, SKEL_Spine3, SKEL_Pelvis,
					SKEL_L_Clavicle, SKEL_R_Clavicle, SKEL_L_UpperArm, SKEL_R_UpperArm,
					SKEL_L_Forearm, SKEL_R_Forearm, SKEL_L_Hand, SKEL_R_Hand,
					SKEL_L_Finger02, SKEL_R_Finger02, SKEL_L_Finger12, SKEL_R_Finger12,
					SKEL_L_Finger42, SKEL_R_Finger42, SKEL_L_Thigh, SKEL_R_Thigh,
					SKEL_L_Calf, SKEL_R_Calf, SKEL_L_Foot, SKEL_R_Foot,
					SKEL_L_Toe0, SKEL_R_Toe0
				};
				int BodyIndices[sizeof(BodyBones) / sizeof(BodyBones[0])];
				int BodyCount = 0, HeadIndex = 0;
				const bool Humanoid = GetBoneIndex(Skel, SKEL_Head, HeadIndex);
				if (Humanoid)
					for (const auto Mask : BodyBones) {
						int Index = 0;
						if (GetBoneIndex(Skel, Mask, Index) && Index >= 0 && Index < NumBones)
							BodyIndices[BodyCount++] = Index;
					}
				const bool UseBody = BodyCount >= 8;
				D3DXMATRIX View;
				if (!Pointers::pViewPort || !Mem.ReadBuffer(Pointers::pViewPort + 0x24C, &View, sizeof(View)))
					return false;
				D3DXMatrixTranspose(&View, &View);
				for (int Bone = 0; Bone < (UseBody ? BodyCount : NumBones); ++Bone)

				{
					const D3DXMATRIX& B = BoneMtx[UseBody ? BodyIndices[Bone] : Bone];
					// Osso nao preenchido: a matriz vem zerada.
					if ( B._41 == 0.f && B._42 == 0.f && B._43 == 0.f )
						continue;

					// Mesma conta do GetBonePosInstFrag: a translacao do osso
					// levada para o mundo pela matriz da entidade.
					const D3DXVECTOR3 World(
						M._11 * B._41 + M._21 * B._42 + M._31 * B._43 + M._41,
						M._12 * B._41 + M._22 * B._42 + M._32 * B._43 + M._42,
						M._13 * B._41 + M._23 * B._42 + M._33 * B._43 + M._43 );

					const D3DXVECTOR2 P = ProjectWorldToScreen(World, View);

					// WorldToScreen devolve (0,0) para o que esta atras da
					// camera: esse osso nao tem posicao de tela valida.
					if (!std::isfinite(P.x) || !std::isfinite(P.y) || (P.x == 0.f && P.y == 0.f))
						continue;

					if ( P.x < MinX ) MinX = P.x;
					if ( P.x > MaxX ) MaxX = P.x;
					if ( P.y < MinY ) MinY = P.y;
					if ( P.y > MaxY ) MaxY = P.y;

					++Valid;
				}

				if ( Valid < 4 )
					return false;

				Out.Left = MinX;
				Out.Right = MaxX;
				Out.Top = MinY;
				Out.Bottom = MaxY;
				Out.Width = MaxX - MinX;
				Out.Height = MaxY - MinY;

				// O esqueleto do MESMO ped e desenhado logo em seguida e pode
				// reaproveitar estas matrizes.
				g_BoneSkel = Skel;
				g_BoneOwner = Ped;
				return true;
			}




			inline std::unordered_map<uintptr_t, std::unordered_map<unsigned int, int>> StoredBonesIdx;
			inline std::mutex BonesCache;


			inline bool GetBoneIndex( const cSkeleton_t & cSkeleton, unsigned int boneId, int & outBoneIdx )
			{
				std::lock_guard<std::mutex> lock( BonesCache );
				{
					// Referencia, nao copia. Com "auto" o mapa inteiro era copiado
					// a cada chamada e as escritas abaixo caiam na copia, entao o
					// cache nunca acertava e toda busca refazia a caminhada na
					// tabela hash do jogo -- varias syscalls por osso, por ped,
					// por frame.
					auto & StoredBonesIdxCache = StoredBonesIdx[ cSkeleton.crSkeletonData.Ptr ];

					if ( StoredBonesIdxCache.find( boneId ) != StoredBonesIdxCache.end( ) ) {
						outBoneIdx = StoredBonesIdxCache[ boneId ];
						return true;
					}


					if ( cSkeleton.crSkeletonData.m_Used != 0 ) // crSkeletonData->m_BoneIdTable.m_Used
					{
						unsigned short m_BoneIdTable_Slots = cSkeleton.crSkeletonData.m_BoneIdTable_Slots; // crSkeletonData->m_BoneIdTable.m_Slots

						std::uintptr_t m_BoneIdTable_Hash = Mem.Read<std::uintptr_t>( cSkeleton.crSkeletonData.m_BoneIdTable + 0x8 * ( boneId % ( unsigned int ) m_BoneIdTable_Slots ) );
						for ( std::uintptr_t i = m_BoneIdTable_Hash; i != 0; i = Mem.Read<std::uintptr_t>( i + 0x8 ) )
						{
							int i_key = Mem.Read<int>( i );
							if ( boneId == i_key )
							{
								int p_Data = Mem.Read<int>( i + 0x4 );
								if ( p_Data )
								{
									outBoneIdx = p_Data;
									StoredBonesIdxCache[ boneId ] = p_Data;
									return true;
								}
							}
						}
					}
					else if ( boneId < cSkeleton.crSkeletonData.m_NumBones )
					{
						outBoneIdx = boneId;
						StoredBonesIdxCache[ boneId ] = boneId;

						return true;
					}

					return false;
				}
			}

			inline D3DXVECTOR3 GetBonePosInstFrag( std::uintptr_t InstFrag, unsigned int BoneID, D3DXMATRIX Arg1, uintptr_t Arg2 )
			{
				D3DXMATRIX v4; // r8
				D3DXMATRIX Result; // rax

				v4 = Arg1;
				if ( !v4 ) { return D3DXVECTOR3( 0, 0, 0 ); }

				Result = Mem.Read<D3DXMATRIX>( Arg2 + ( ( unsigned __int64 ) BoneID << 6 ) );
				if ( !Result ) { return D3DXVECTOR3( 0, 0, 0 ); }

				D3DXVECTOR3 vec1( v4._11, v4._12, v4._13 );
				D3DXVECTOR3 vec2( v4._21, v4._22, v4._23 );
				D3DXVECTOR3 vec3( v4._31, v4._32, v4._33 );
				D3DXVECTOR3 vec4( v4._41, v4._42, v4._43 );
				D3DXVECTOR3 vec5( Result._41, Result._42, Result._43 );
				return D3DXVECTOR3(
					vec1.x * vec5.x + vec4.x + vec2.x * vec5.y + vec3.x * vec5.z,
					vec1.y * vec5.x + vec4.y + vec2.y * vec5.y + vec3.y * vec5.z,
					vec1.z * vec5.x + vec4.z + vec2.z * vec5.y + vec3.z * vec5.z
				);
			}

			inline D3DXVECTOR3 GetBonePosComplex( CPed * Ped, unsigned int Mask, const cSkeleton_t & cSkeleton_t )
			{
				bool result = false;

				int BoneId = 0;

				if ( GetBoneIndex( cSkeleton_t, Mask, BoneId ) ) {
					return GetBonePosInstFrag( cSkeleton_t.m_pSkeleton, BoneId, cSkeleton_t.Arg1, cSkeleton_t.Arg2 );
				}

				return Ped->GetPos( );
			}

			// Resolve as hitboxes usadas pelo Aimbot e pelo Silent Aim.
			//
			// Todos os 3 hitboxes usam BoneMask real do skeleton:
			//   Head  = SKEL_Head  (0x796E) — era 0x00 antes, que e SKEL_ROOT
			//           (pelvis), por isso o aimbot mirava "no peito".
			//   Neck  = SKEL_Neck_1 (0x9995)
			//   Torso = SKEL_Spine3 (0x60F2) — upper chest, hitbox grande.
			// O skeleton e carregado para o ped solicitado em vez de depender
			// do cache g_BoneOwner do ESP, que pode pertencer a outro ped.
			inline bool GetAimHitboxPosition( CPed * Ped, int Hitbox, D3DXVECTOR3 & Out )
			{
				if ( !Ped )
					return false;

				const unsigned int Mask =
					Hitbox == 1 ? SKEL_Neck_1 :
					Hitbox == 2 ? SKEL_Spine3 :
					SKEL_Head;  // 0 (default) e qualquer outro → Head

				cSkeleton_t Skeleton{};
				if ( !LoadSkeleton( Ped, Skeleton ) || !Skeleton.Arg2 )
					return false;

				const int NumBones = static_cast<int>( Skeleton.crSkeletonData.m_NumBones );
				int BoneIndex = -1;
				if ( NumBones <= 0 || NumBones > 512 ||
					!GetBoneIndex( Skeleton, Mask, BoneIndex ) ||
					BoneIndex < 0 || BoneIndex >= NumBones )
					return false;

				Out = GetBonePosInstFrag(
					Skeleton.m_pSkeleton, BoneIndex, Skeleton.Arg1, Skeleton.Arg2 );

				return std::isfinite( Out.x ) && std::isfinite( Out.y ) &&
					std::isfinite( Out.z ) &&
					( Out.x != 0.f || Out.y != 0.f || Out.z != 0.f );
			}

			// Posicao de um osso a partir do buffer que CalculateDynamicPedBounds
			// ja leu, sem tocar no processo de novo.
			//
			// O ESP desenha a caixa e o esqueleto do mesmo ped em sequencia,
			// entao quando chega no esqueleto as matrizes ja estao na mao. Pedir
			// osso a osso ali eram 14 leituras por ped, por frame, para dados
			// que acabaram de ser lidos.
			inline D3DXVECTOR3 GetBonePosFast( CPed * Ped, unsigned int Mask )
			{
				if ( Ped != g_BoneOwner )
					return Ped->GetPos( );

				int Idx = 0;

				if ( !GetBoneIndex( g_BoneSkel, Mask, Idx ) )
					return Ped->GetPos( );

				if ( Idx < 0 || Idx >= ( int ) g_BoneMtx.size( ) )
					return Ped->GetPos( );

				const D3DXMATRIX & B = g_BoneMtx[ Idx ];
				const D3DXMATRIX & M = g_BoneSkel.Arg1;

				return D3DXVECTOR3(
					M._11 * B._41 + M._21 * B._42 + M._31 * B._43 + M._41,
					M._12 * B._41 + M._22 * B._42 + M._32 * B._43 + M._42,
					M._13 * B._41 + M._23 * B._42 + M._33 * B._43 + M._43 );
			}


			inline std::string GetPedName(CPed* Ped)
			{
				for (int i = 0; i < SDK::Game::EntityList.size(); i++)
				{
					uintptr_t NetPointer = Mem.Read<uintptr_t>(
						g_Offsets.m_CitizenNamesModBase + (i * 0x8)
					);

					if (!NetPointer)
						continue;

					for (auto& Ent : EntityList)
					{
						if (Ent.Ped == Ped)
						{
							std::string Name = Mem.ReadString(NetPointer + 0x18);
							if (!Name.empty())
								Ent.NetworkInfo.UserName = Name;
							break;
						}
					}
				}

				for (auto& Ent : EntityList)
				{
					if (Ent.Ped != Ped)
						continue;

					if (!Ent.NetworkInfo.UserName.empty())
						return Ent.NetworkInfo.UserName;
				}

				return xorstr("Ped");
			}

			inline CPed* GetClosestPed(int MaxDistance, bool IgnoreNpc, bool VisibleCheck)
			{
				CPed* ClosestPed = nullptr;
				float ClosestDistSq = FLT_MAX;

				D3DXMATRIX ViewMatrix{};
				if (!Core::SDK::Pointers::pViewPort ||
					!Mem.ReadBuffer(Core::SDK::Pointers::pViewPort + 0x24C,
						&ViewMatrix, sizeof(ViewMatrix)))
					return nullptr;
				D3DXMatrixTranspose(&ViewMatrix, &ViewMatrix);

				// Chamado a cada 1ms pelo silent/aimbot/trigger, na mesma
				// frequencia em que a thread de fundo troca o vetor. O loop e
				// curto (so contas), entao segurar o lock aqui e barato.
				std::lock_guard<std::mutex> Lock(Core::SDK::Game::EntityListMutex);

				for (auto& Entity : Core::SDK::Game::EntityList)
				{
					CPed* Ped = Entity.Ped;

					if (Ped == Core::SDK::Pointers::pLocalPlayer)
						continue;

					if (std::abs(Entity.Health) <= 101.f)
						continue;

					if (IgnoreNpc && Entity.PedType != 2)
						continue;

					if (Entity.IsFriend)
						continue;

					if (Entity.Distance > MaxDistance)
						continue;

					if (VisibleCheck && !Entity.IsVisible)
						continue;

					D3DXVECTOR2 PedPos = ProjectWorldToScreen(Entity.Pos, ViewMatrix);
					if (PedPos.x == 0.f && PedPos.y == 0.f)
						continue;

					float x = PedPos.x - g_Variables.g_vGameWindowCenter.x;
					float y = PedPos.y - g_Variables.g_vGameWindowCenter.y;
					float AimDistSq = (x * x) + (y * y);

					if (AimDistSq < ClosestDistSq)
					{
						ClosestDistSq = AimDistSq;
						ClosestPed = Ped;
					}
				}

				return ClosestPed;
			}

			// Sticky Target: revalida UM ped ja escolhido com exatamente os
			// mesmos filtros do GetClosestPed acima. E uma funcao separada e
			// nao um flag la dentro porque o sticky nao quer o "mais perto do
			// crosshair" -- quer saber se o alvo atual ainda serve. Se um
			// filtro mudar la em cima, tem que mudar aqui junto.
			inline bool IsPedStillValid(CPed* Ped, int MaxDistance, bool IgnoreNpc, bool VisibleCheck)
			{
				if (!Ped || Ped == Core::SDK::Pointers::pLocalPlayer)
					return false;

				std::lock_guard<std::mutex> Lock(Core::SDK::Game::EntityListMutex);

				for (auto& Entity : Core::SDK::Game::EntityList)
				{
					if (Entity.Ped != Ped)
						continue;

					if (std::abs(Entity.Health) <= 101.f)  return false;
					if (IgnoreNpc && Entity.PedType != 2)  return false;
					if (Entity.IsFriend)                   return false;
					if (Entity.Distance > MaxDistance)     return false;
					if (VisibleCheck && !Entity.IsVisible) return false;

					return true;
				}

				// Sumiu da lista: morreu, despawnou ou saiu do streaming.
				return false;
			}

			inline CVehicle* GetClosestVehicle(int MaxDistance)
			{
				for (const auto& Vehicle : Core::SDK::Game::VehicleList)
				{
					if (!Vehicle.Pointer)
						continue;

					if (Vehicle.Dist > MaxDistance)
						break;

					return Vehicle.Pointer;
				}

				return nullptr;
			}

			struct VehicleWheelTarget
			{
				CVehicle* Vehicle = nullptr;
				uintptr_t Wheel = 0;
				int WheelIndex = -1;
				D3DXVECTOR3 Center{};
				D3DXVECTOR3 World{};
				D3DXVECTOR2 Screen{};
				D3DXVECTOR3 VehicleOrigin{};
				float Radius = 0.f;
				float VehicleDistance = 0.f;
				float ScreenDistance = FLT_MAX;
				bool UsedFallback = false;
			};

			namespace VehicleWheelDetail
			{
				// Confirmado no IDA b3258:
				// CVehicle::m_ppWheels = 0xC20, m_nNumWheels = 0xC28.
				// CWheel::m_vecHitCentrePos = 0x50 e m_vecHitPos = 0x40.
				constexpr uintptr_t VehicleWheelsOffset = 0xC20;
				constexpr uintptr_t VehicleWheelCountOffset = 0xC28;
				constexpr uintptr_t WheelHitPosOffset = 0x40;
				constexpr uintptr_t WheelCentreOffset = 0x50;
				constexpr uintptr_t WheelRadiusOffset = 0x110;
				constexpr uintptr_t WheelWidthOffset = 0x11C;
				constexpr int MaxWheels = 12;

				struct WheelSample
				{
					uintptr_t Pointer = 0;
					D3DXVECTOR3 Position{};
					float Radius = 0.36f;
					float Width = 0.24f;
					bool Fallback = false;
				};

				struct WheelCacheEntry
				{
					std::array<WheelSample, MaxWheels> Wheels{};
					D3DXMATRIX VehicleMatrix{};
					bool HasVehicleMatrix = false;
					int Count = 0;
					std::chrono::steady_clock::time_point Updated{};
				};

				inline std::mutex CacheMutex;
				inline std::unordered_map<uintptr_t, WheelCacheEntry> Cache;

				inline bool Finite(const D3DXVECTOR3& Value)
				{
					return std::isfinite(Value.x) && std::isfinite(Value.y) &&
						std::isfinite(Value.z);
				}

				inline bool Plausible(const D3DXVECTOR3& Value, const D3DXVECTOR3& VehiclePos)
				{
					if (!Finite(Value) ||
						(Value.x == 0.f && Value.y == 0.f && Value.z == 0.f))
						return false;

					const D3DXVECTOR3 Delta = Value - VehiclePos;
					const float DistanceSq = Delta.x * Delta.x + Delta.y * Delta.y + Delta.z * Delta.z;
					return DistanceSq > 0.01f && DistanceSq < 100.f;
				}

				inline int BuildFallback(const VehicleStructure& Vehicle, WheelCacheEntry& Entry)
				{
					D3DXMATRIX Matrix{};
					if (!Mem.ReadBuffer(reinterpret_cast<uintptr_t>(Vehicle.Pointer) + 0x60,
						&Matrix, sizeof(Matrix)))
						return 0;
					Entry.VehicleMatrix = Matrix;
					Entry.HasVehicleMatrix = true;

					// Quatro cantos na altura dos pneus. So entra quando o jogo ainda
					// nao publicou CWheel (LOD/streaming); nunca volta ao centro do carro.
					const D3DXVECTOR3 Local[] = {
						D3DXVECTOR3(-0.85f,  1.25f, -0.45f),
						D3DXVECTOR3( 0.85f,  1.25f, -0.45f),
						D3DXVECTOR3(-0.85f, -1.25f, -0.45f),
						D3DXVECTOR3( 0.85f, -1.25f, -0.45f)
					};

					Entry.Count = 0;
					for (const auto& Point : Local)
					{
						D3DXVECTOR4 World{};
						D3DXVec3Transform(&World, &Point, &Matrix);
						D3DXVECTOR3 Position(World.x, World.y, World.z);
						if (!Plausible(Position, Vehicle.Pos))
							continue;
						Entry.Wheels[Entry.Count++] = { 0, Position, 0.36f, 0.24f, true };
					}
					return Entry.Count;
				}

				inline bool ReadWheels(const VehicleStructure& Vehicle, WheelCacheEntry& Out)
				{
					if (RunningInsideBraveHost()) return false;
					const auto Now = std::chrono::steady_clock::now();
					const uintptr_t VehicleAddress = reinterpret_cast<uintptr_t>(Vehicle.Pointer);

					std::lock_guard<std::mutex> Guard(CacheMutex);
					auto& Entry = Cache[VehicleAddress];
                    // A posicao fisica da roda deve acompanhar a cadencia dos
                    // loops de mira; cache maior que 1 ms produzia atraso
                    // perceptivel em veiculos em movimento.
                    if (Entry.Count > 0 && Now - Entry.Updated < std::chrono::microseconds(1000))
					{
						Out = Entry;
						return true;
					}

					Entry = {};
					Entry.Updated = Now;
					Entry.HasVehicleMatrix = Mem.ReadBuffer(VehicleAddress + 0x60,
						&Entry.VehicleMatrix, sizeof(Entry.VehicleMatrix));

					uintptr_t WheelArray = 0;
					int WheelCount = 0;
					if (Mem.ReadBuffer(VehicleAddress + VehicleWheelsOffset, &WheelArray, sizeof(WheelArray)) &&
						Mem.ReadBuffer(VehicleAddress + VehicleWheelCountOffset, &WheelCount, sizeof(WheelCount)) &&
						WheelArray && WheelCount > 0 && WheelCount <= MaxWheels)
					{
						std::array<uintptr_t, MaxWheels> WheelPointers{};
						if (Mem.ReadBuffer(WheelArray, WheelPointers.data(),
							static_cast<size_t>(WheelCount) * sizeof(uintptr_t)))
						{
							for (int Index = 0; Index < WheelCount; ++Index)
							{
								const uintptr_t Wheel = WheelPointers[Index];
								if (!Wheel) continue;

								D3DXVECTOR3 Position{};
								bool Valid = Mem.ReadBuffer(Wheel + WheelCentreOffset,
									&Position, sizeof(Position)) && Plausible(Position, Vehicle.Pos);
								if (!Valid)
								{
									Valid = Mem.ReadBuffer(Wheel + WheelHitPosOffset,
										&Position, sizeof(Position)) && Plausible(Position, Vehicle.Pos);
									if (Valid) Position.z += 0.20f;
								}
								if (Valid)
								{
									float Radius = Mem.Read<float>(Wheel + WheelRadiusOffset);
									float Width = Mem.Read<float>(Wheel + WheelWidthOffset);
									if (!std::isfinite(Radius) || Radius < 0.12f || Radius > 1.5f)
										Radius = 0.36f;
									if (!std::isfinite(Width) || Width < 0.05f || Width > 1.0f)
										Width = 0.24f;
									Entry.Wheels[Entry.Count++] = { Wheel, Position, Radius, Width, false };
								}
							}
						}
					}

					if (!Entry.Count)
						BuildFallback(Vehicle, Entry);

					Out = Entry;
					return Entry.Count > 0;
				}

				inline D3DXVECTOR3 Normalized(const D3DXVECTOR3& Value,
					const D3DXVECTOR3& Fallback)
				{
					D3DXVECTOR3 Result = Value;
					const float Length = D3DXVec3Length(&Result);
					if (!std::isfinite(Length) || Length < 0.0001f)
						return Fallback;
					return Result / Length;
				}

				// Mira na borracha exposta, e nao no centro metalico do cubo. O ponto
				// escolhido fica na face do pneu voltada ao jogador e no quadrante
				// visivel da banda lateral. Isso evita que carroceria, solo, areia ou
				// agua recebam o impacto antes do pneu.
				inline D3DXVECTOR3 TireImpactPoint(const WheelSample& Wheel,
					const WheelCacheEntry& Sample, const D3DXVECTOR3& Shooter)
				{
					const D3DXVECTOR3 DefaultRight(1.f, 0.f, 0.f);
					const D3DXVECTOR3 DefaultUp(0.f, 0.f, 1.f);
					D3DXVECTOR3 Right = DefaultRight;
					D3DXVECTOR3 Up = DefaultUp;

					if (Sample.HasVehicleMatrix)
					{
						D3DXVECTOR3 LocalRight(1.f, 0.f, 0.f);
						D3DXVECTOR3 LocalUp(0.f, 0.f, 1.f);
						D3DXVec3TransformNormal(&Right, &LocalRight, &Sample.VehicleMatrix);
						D3DXVec3TransformNormal(&Up, &LocalUp, &Sample.VehicleMatrix);
						Right = Normalized(Right, DefaultRight);
						Up = Normalized(Up, DefaultUp);
					}

					const D3DXVECTOR3 ToShooter = Normalized(Shooter - Wheel.Position, Up);
					const float SideSign = D3DXVec3Dot(&ToShooter, &Right) >= 0.f ? 1.f : -1.f;
					const float AxleProjection = D3DXVec3Dot(&ToShooter, &Right);
					D3DXVECTOR3 RingDirection = ToShooter - Right * AxleProjection + Up * 0.30f;
					RingDirection = Normalized(RingDirection, Up);

					const float FaceOffset = (Wheel.Width * 0.48f < 0.30f)
						? Wheel.Width * 0.48f : 0.30f;
					float RingOffset = Wheel.Radius * 0.68f;
					if (RingOffset < 0.14f) RingOffset = 0.14f;
					if (RingOffset > 0.55f) RingOffset = 0.55f;

					return Wheel.Position + Right * (SideSign * FaceOffset) +
						RingDirection * RingOffset;
				}
			}

			// Seleciona uma roda real, nao o centro do veiculo. PreferredVehicle e
			// PreferredWheel mantem o alvo estavel enquanto a tecla esta pressionada.
			inline bool GetClosestVehicleWheel(int MaxDistance, float FovPixels,
				VehicleWheelTarget& Out, CVehicle* PreferredVehicle = nullptr,
				uintptr_t PreferredWheel = 0,
				const std::function<bool(const VehicleWheelTarget&)>& Accept = {})
			{
				Out = {};
				D3DXMATRIX ViewMatrix{};
				if (!Core::SDK::Pointers::pViewPort ||
					!Mem.ReadBuffer(Core::SDK::Pointers::pViewPort + 0x24C,
						&ViewMatrix, sizeof(ViewMatrix)))
					return false;
				D3DXMatrixTranspose(&ViewMatrix, &ViewMatrix);

				std::vector<VehicleStructure> Vehicles;
				{
					std::lock_guard<std::mutex> Lock(Core::SDK::Game::VehicleListMutex);
					Vehicles = Core::SDK::Game::VehicleList;
				}

				const float FovSq = FovPixels * FovPixels;
				float BestDistanceSq = FLT_MAX;
				const D3DXVECTOR3 Shooter = Core::SDK::Pointers::pLocalPlayer
					? Core::SDK::Pointers::pLocalPlayer->GetPos() : D3DXVECTOR3{};

				auto Evaluate = [&](const VehicleStructure& Vehicle, bool PreferredOnly)
				{
					if (!Vehicle.Pointer || Vehicle.Dist > MaxDistance)
						return false;
					if (Core::SDK::Pointers::pLocalPlayer &&
						Vehicle.Pointer == Core::SDK::Pointers::pLocalPlayer->GetLastVehicle() &&
						Core::SDK::Pointers::pLocalPlayer->InVehicle())
						return false;

					VehicleWheelDetail::WheelCacheEntry Sample{};
					if (!VehicleWheelDetail::ReadWheels(Vehicle, Sample))
						return false;

					bool Found = false;
					for (int Index = 0; Index < Sample.Count; ++Index)
					{
						const auto& Wheel = Sample.Wheels[Index];
						if (PreferredOnly && PreferredWheel && Wheel.Pointer != PreferredWheel)
							continue;

						const D3DXVECTOR3 ImpactPoint = VehicleWheelDetail::TireImpactPoint(
							Wheel, Sample, Shooter);
						const D3DXVECTOR2 Screen = ProjectWorldToScreen(ImpactPoint, ViewMatrix);
						if (!std::isfinite(Screen.x) || !std::isfinite(Screen.y) ||
							(Screen.x == 0.f && Screen.y == 0.f))
							continue;

						const float Dx = Screen.x - g_Variables.g_vGameWindowCenter.x;
						const float Dy = Screen.y - g_Variables.g_vGameWindowCenter.y;
						const float DistanceSq = Dx * Dx + Dy * Dy;
						if (DistanceSq > FovSq || DistanceSq >= BestDistanceSq)
							continue;

						VehicleWheelTarget Candidate{};
						Candidate.Vehicle = Vehicle.Pointer;
						Candidate.Wheel = Wheel.Pointer;
						Candidate.WheelIndex = Index;
						Candidate.Center = Wheel.Position;
						Candidate.World = ImpactPoint;
						Candidate.Screen = Screen;
						Candidate.VehicleOrigin = Vehicle.Pos;
						Candidate.Radius = Wheel.Radius;
						Candidate.VehicleDistance = Vehicle.Dist;
						Candidate.ScreenDistance = std::sqrt(DistanceSq);
						Candidate.UsedFallback = Wheel.Fallback;
						if (Accept && !Accept(Candidate))
							continue;

						BestDistanceSq = DistanceSq;
						Out = Candidate;
						Found = true;
					}
					return Found;
				};

				if (PreferredVehicle)
				{
					for (const auto& Vehicle : Vehicles)
						if (Vehicle.Pointer == PreferredVehicle)
						{
							if (Evaluate(Vehicle, PreferredWheel != 0))
								return true;
							if (PreferredWheel && Evaluate(Vehicle, false))
								return true;
							break;
						}
				}

				for (const auto& Vehicle : Vehicles)
					Evaluate(Vehicle, false);
				return Out.Vehicle != nullptr;
			}

			// Conserva o alvo durante falhas muito curtas de streaming/contato.
			// A posicao acompanha a translacao do veiculo, portanto nao fica parada
			// no mundo quando a roda some do buffer por alguns frames.
			inline bool RefreshVehicleWheelTarget(VehicleWheelTarget& Target,
				float FovPixels, float FovSlack = 1.35f)
			{
				if (!Target.Vehicle)
					return false;

				const D3DXVECTOR3 Origin = Target.Vehicle->GetPos();
				if (!VehicleWheelDetail::Finite(Origin))
					return false;

				const D3DXVECTOR3 Delta = Origin - Target.VehicleOrigin;
				const float DeltaSq = Delta.x * Delta.x + Delta.y * Delta.y + Delta.z * Delta.z;
				if (!std::isfinite(DeltaSq) || DeltaSq > 100.f)
					return false;

				Target.Center += Delta;
				Target.World += Delta;
				Target.VehicleOrigin = Origin;
				Target.Screen = WorldToScreen(Target.World);
				if (!std::isfinite(Target.Screen.x) || !std::isfinite(Target.Screen.y) ||
					(Target.Screen.x == 0.f && Target.Screen.y == 0.f))
					return false;

				const float Dx = Target.Screen.x - g_Variables.g_vGameWindowCenter.x;
				const float Dy = Target.Screen.y - g_Variables.g_vGameWindowCenter.y;
				Target.ScreenDistance = std::sqrt(Dx * Dx + Dy * Dy);
				return Target.ScreenDistance <= FovPixels * FovSlack;
			}

			inline CVehicle* GetClosestVehicleWheel(int MaxDistance, float FovPixels,
				D3DXVECTOR3& OutPos)
			{
				VehicleWheelTarget Target{};
				if (!GetClosestVehicleWheel(MaxDistance, FovPixels, Target))
					return nullptr;
				OutPos = Target.World;
				return Target.Vehicle;
			}

			inline bool IsOnScreen( D3DXVECTOR2 Pos )
			{
				if ( Pos == D3DXVECTOR2( 0, 0 ) ) return false;
				return true;
			}
		}
	}

}
